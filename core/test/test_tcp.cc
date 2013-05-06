#include <iostream>
#include <boost/pointer_cast.hpp>
#include <sys/time.h>

#include "core/test/unit-test.h"
#include "core/util.hpp"
#include "core/tcpserver.h"

using namespace std;
using namespace dh_core;

//............................................................ basictcptest ....

class BasicTCPTest : public TCPServerClient, public TCPChannelClient
{
public:

    static const uint32_t MAX_ITERATION = 20;
    static const uint32_t WBUFFERSIZE = 4 * 1024;  // 4 KiB
    static const uint32_t TIMEINTERVAL_MS = 1 * 1000;   // 1 s

    //.... create/destroy ....//

    BasicTCPTest()
        : log_("testtcp/")
        , addr_(SocketAddress::GetAddr("127.0.0.1", 9999 + (rand() % 100)))
        , server_ch_(NULL)
        , client_ch_(NULL)
        , count_(0)
        , iter_(0)
        , rbuf_(IOBuffer::Alloc(WBUFFERSIZE))
    {
    }

    ~BasicTCPTest()
    {
        // TODO: trash the buffers
    }

    void Start(int nonce)
    {
        epoll_ = new EpollSet("serverEpoll/");
        tcpServer_ = new TCPServer(addr_.RemoteAddr(), epoll_);
        tcpServer_->Accept(this, make_cb(this, &BasicTCPTest::AcceptStarted));
    }

    //.... handlers ....//

    __async__ void AcceptStarted(int status)
    {
        ASSERT(status == 0);

        tcpClient_ = new TCPConnector(epoll_);
        ThreadPool::Schedule(tcpClient_, &TCPConnector::Connect, addr_,
                             make_cb(this, &BasicTCPTest::HandleClientConn));
    }

    __async__ virtual void TCPServerHandleConnection(int status, TCPChannel * ch)
    {
        ASSERT(status == 0);

        INFO(log_) << "Accepted.";

        server_ch_ = ch;
        server_ch_->RegisterClient(this);

        server_ch_->Read(rbuf_);
    }

    __async__ virtual void HandleClientConn(int status, TCPChannel * ch)
    {
        ASSERT(status == 0);

        INFO(log_) << "Connected.";

        client_ch_ = ch;
        client_ch_->RegisterClient(this);

        SendData();
    }

    __async__ virtual void TcpReadDone(TCPChannel * ch, int status, IOBuffer buf)
    {
        ASSERT((size_t) status == rbuf_.Size());
        ASSERT(buf == rbuf_);

        VerifyData(rbuf_);
        ch->Read(rbuf_);
   }

    __async__ virtual void TcpWriteDone(TCPChannel *, int status)
    {
        ASSERT(status > 0 &&  ((size_t) status <= wbuf_.Size()));

        if ((size_t) status < wbuf_.Size()) {
            wbuf_.Cut(status);
            return;
        }

        INFO(log_) << "ClientWriteDone.";

        wbuf_.Trash();
        SendData();
    }

private: 

    void VerifyData(IOBuffer & buf)
    {
        const uint32_t cksum = Adler32::Calc(buf.Ptr(), buf.Size());

        INVARIANT(cksum_.front() == cksum);
        cksum_.pop_front();

        DEBUG(log_) << "POP NEXT:" << (int) cksum_.front()
                    << " EMPTY:" << cksum_.empty();

        if (cksum_.empty() && iter_ > MAX_ITERATION) {
            ASSERT(!"Stop");
            // we have reached our sending limit
            return;
        }
    }

    int MBps(uint64_t bytes, uint64_t ms)
    {
        return (bytes / (1024 * 1024)) / (ms / 1000);
    }

    void SendData()
    {
        if (iter_ > MAX_ITERATION) {
            // we have reached our sending limit
            return;
        }

        INFO(log_) << "SendData.";

        ASSERT(!wbuf_);
        wbuf_ = IOBuffer::Alloc(WBUFFERSIZE);
        wbuf_.FillRandom();

        const uint32_t cksum = Adler32::Calc(wbuf_.Ptr(), wbuf_.Size());
        DEBUG(log_) << "PUSH " << (uint32_t) cksum;
        cksum_.push_back(cksum);

        INVARIANT(client_ch_->EnqueueWrite(wbuf_));

        ++iter_;
    }

    LogPath log_;
    SocketAddress addr_;
    EpollSet * epoll_;
    TCPServer * tcpServer_;
    TCPConnector * tcpClient_;
    TCPChannel * server_ch_;
    TCPChannel * client_ch_;
    list<uint32_t> cksum_;
    uint32_t count_;
    uint32_t iter_;
    Adler32 adler32_;
    IOBuffer wbuf_;
    IOBuffer rbuf_;
};

void
test_tcp_basic()
{
    ThreadPool::Start(/*ncores=*/ 4);

    BasicTCPTest test;
    ThreadPool::Schedule(&test, &BasicTCPTest::Start, /*nonce=*/ 0);

    ThreadPool::Wait();
}

//.................................................................... main ....

int
main(int argc, char ** argv)
{
    srand(time(NULL));

    InitTestSetup();

    TEST(test_tcp_basic);

    TeardownTestSetup();

    return 0;
}
