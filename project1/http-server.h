class HTTPServer
{
    public:
        HTTPServer();
        int startServer();
        int getSocketFileDescriptor() const;
        void waitForConnection() const;
        int acceptConnection() const;

    private:
        int d_sockfd;
};
