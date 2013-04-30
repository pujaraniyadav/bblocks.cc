#include <boost/program_options.hpp>
#include <string>
#include <iostream>

#include "core/tcpserver.h"
#include "core/test/unit-test.h"

using namespace std;
using namespace dh_core;

namespace po = boost::program_options;

struct ChStats
{
    ChStats()
        : start_ms_(NowInMilliSec()), bytes_read_(0), bytes_written_(0)
    {
    }

    uint64_t start_ms_;
    uint64_t bytes_read_;
    uint64_t bytes_written_;
};

//...................................................... TCPServerBenchmark ....

/**
 */
class TCPServerBenchmark : public TCPServerClient, public TCPChannelClient
{
public:

    TCPServerBenchmark(const sockaddr_in & addr)
        : epoll_("/server"), server_(addr, &epoll_)
    {
    }

    virtual ~TCPServerBenchmark()
    {
    }

    void Start(int)
    {
        // Start listening
        ThreadPool::Schedule(&server_, &TCPServer::Accept,
                             static_cast<TCPServerClient *>(this),
                             static_cast<Callback<status_t> *>(NULL));
    }

    virtual void TCPServerHandleConnection(int status, TCPChannel * ch)
    {
        cout << "Got ch " << ch << endl;

        {
            AutoLock _(&lock_);
            chstats_.insert(make_pair(ch, ChStats()));
        }

        ch->InitChannel(this);
    }

    virtual void TCPChannelHandleRead(TCPChannel * ch, int status,
                                      DataBuffer * buf)
    {
        {
            AutoLock _(&lock_);

            auto it = chstats_.find(ch);
            ASSERT(it != chstats_.end());
            it->second.bytes_read_ += buf->Size();
        }

        delete buf;
    }

private:

    typedef map<TCPChannel *, ChStats> chstats_map_t;

    SpinMutex lock_;
    EpollSet epoll_;
    TCPServer server_;
    chstats_map_t chstats_;
};

//...................................................... TCPClientBenchmark ....

/**
 */
class TCPClientBenchmark : public TCPChannelClient
{
public:

    /*.... create/destroy ....*/

    TCPClientBenchmark(const SocketAddress & addr, const size_t iosize,
                       const size_t nconn, const size_t nsec)
        : epoll_("/client"), connector_(&epoll_), addr_(addr)
        , iosize_(iosize), nconn_(nconn), nsec_(nsec)
    {
        buf_ = new DataBuffer();
        RawData data(iosize_);
        data.FillRandom();
        buf_->Append(data);
    }

    virtual ~TCPClientBenchmark()
    {
        delete buf_;
    }

    void Start(int)
    {
        for (size_t i = 0; i < nconn_; ++i) {
            nactiveconn_.Add(/*count=*/ 1);
            ThreadPool::Schedule(&connector_, &TCPConnector::Connect, addr_,
                                 make_cb(this, &TCPClientBenchmark::Connected));
        }
    }

    /*.... handler functions ....*/

    virtual void TCPChannelHandleRead(TCPChannel *, int, DataBuffer *)
    {
        ASSERT(!"Not Reached");
    }

    /*.... callbacks ....*/

    void Connected(int status, TCPChannel * ch)
    {
        ASSERT(status == OK);
        ASSERT(buf_->Size() == iosize_);

        {
            AutoLock _(&lock_);
            chstats_.insert(make_pair(ch, ChStats()));
        }

        ch->InitChannel(this);
        SendData(ch);
    }

    void WriteDone(int status, TCPChannel * ch)
    {
        ASSERT(status == OK);

        {
            AutoLock _(&lock_);
            auto it = chstats_.find(ch);
            ASSERT(it != chstats_.end());
            it->second.bytes_written_ += iosize_;
        }

        if (timer_.Elapsed() > SEC2MS(nsec_)) {
            nactiveconn_.Add(/*count=*/ -1);
            if (!nactiveconn_.Count()) {
                PrintStats();
                ThreadPool::Shutdown();
            }
            return;
        }

        SendData(ch);
    }

    void PrintStats()
    {
        AutoLock _(&lock_);

        for (auto it = chstats_.begin(); it != chstats_.end(); ++it) {
            cout << "Channel " << it->first << " : " << endl;
            cout << "w-bytes " << it->second.bytes_written_ << " bytes" << endl;
            cout << "time : " << MS2SEC(timer_.Elapsed()) << " s" << endl;
            cout << "write throughput : "
                 << (B2MB(it->second.bytes_written_) / MS2SEC(timer_.Elapsed()))
                 << " MBps" << endl;
        }
    }


private:

    void SendData(TCPChannel * ch)
    {
        ThreadPool::Schedule(ch, &TCPChannel::Write, buf_,
                             make_cb(this, &TCPClientBenchmark::WriteDone, ch));
    }

    typedef map<TCPChannel *, ChStats> chstats_map_t;

    SpinMutex lock_;
    EpollSet epoll_;
    TCPConnector connector_;
    const SocketAddress addr_;
    const size_t iosize_;
    const size_t nconn_;
    const size_t nsec_;
    chstats_map_t chstats_;     //< Stats holder
    DataBuffer * buf_;
    Timer timer_;
    AtomicCounter nactiveconn_;
};


//.................................................................... Main ....

int
main(int argc, char ** argv)
{
    string laddr = "0.0.0.0:0";
    string raddr;
    int iosize = 4 * 1024;
    int nconn = 1;
    int seconds = 60;
    int ncpu = 8;

    po::options_description desc("Options:");
    desc.add_options()
        ("help,h", "Print usage")
        ("server,s", "Start server component")
        ("client,c", "Start client component")
        ("laddr,l", po::value<string>(&laddr),
         "Local address (Default INADDR_ANY:0)")
        ("raddr,r", po::value<string>(&raddr), "Remote address")
        ("iosize,io", po::value<int>(&iosize), "IO size in bytes")
        ("conn,nc", po::value<int>(&nconn), "Client connections (Default 1)")
        ("time,t", po::value(&seconds), "Time in sec (only with -c)")
        ("ncpu,n", po::value<int>(&ncpu), "CPUs to use");

    po::variables_map parg;
    po::store(po::parse_command_line(argc, argv, desc), parg);
    po::notify(parg);

    const bool showusage = parg.count("help")
                           || (parg.count("server") && parg.count("client"))
                           || (!parg.count("server") && !parg.count("client"));

    if (showusage) {
        cerr << "Both -c and -s are provided." << endl;
        cout << desc << endl;
        return -1;
    }

    bool isClientBenchmark = parg.count("client");

    InitTestSetup();
    ThreadPool::Start(ncpu);

    if (isClientBenchmark) {
        cout << "Running benchmark for"
            << " address " << laddr << "->" << raddr
            << " iosize " << iosize << " bytes"
            << " nconn " << nconn
            << " ncpu " << ncpu
            << " seconds " << seconds << " s" << endl;

        ASSERT(!raddr.empty());
        SocketAddress addr = SocketAddress::GetAddr(laddr, raddr);
        TCPClientBenchmark c(addr, iosize, nconn, seconds);
        ThreadPool::Schedule(&c, &TCPClientBenchmark::Start, /*status=*/ 0);
        ThreadPool::Wait();
    } else {
        cout << "Running server at " << laddr
             << " ncpu " << ncpu << endl;

        ASSERT(!laddr.empty());
        TCPServerBenchmark s(SocketAddress::GetAddr(laddr));
        ThreadPool::Schedule(&s, &TCPServerBenchmark::Start, /*status=*/ 0);
        ThreadPool::Wait();
    }

    TeardownTestSetup();
}