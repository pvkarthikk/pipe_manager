#ifndef PIPE_MANAGER_H_INCLUDED
#define PIPE_MANAGER_H_INCLUDED
#include <windows.h>
#include <string>
#include <thread>
#include <map>
#include <queue>
namespace pipe_manager{
    class PipeError: public std::exception{
        private:
            std::string m_err_msg;
        public:
            PipeError(const std::string& err_msg):m_err_msg(err_msg){}
            const char* what() const noexcept override{
                return m_err_msg.c_str();
            }
    };
    enum Pipe_{
         Pipe_Connecting,
         Pipe_Connected,
         Pipe_Disconnecting,
         Pipe_Disconnected,
         Pipe_Error,
         Pipe_Closed
    };
    typedef std::vector<std::string>        msgs_t;
    typedef std::map<std::string, msgs_t>   pipe_msgs_t;
    class Pipe{
        private:
            std::string                 m_pipe_read_name;
            std::string                 m_pipe_write_name;
            HANDLE                      m_handle_read;
            HANDLE                      m_handle_write;
            std::thread                 *p_connect;
            std::thread                 *p_read;
            void                        th_connect();
            void                        th_read();
        public:
            Pipe_                       m_state;
            std::string                 m_name;
            bool                        m_server;
            std::queue<std::string>     m_msg_queue;
            void                        connect();
            bool                        is_connected();
            void                        disconnect();
            int                         write(std::string payload);
            msgs_t                      read(int no_of_msg = -1);
            bool                        has_message();
            Pipe(bool as_server=true, std::string name="default");
            ~Pipe();
    };
    class PipeManager{
        private:
            std::vector<Pipe*>          m_pipes;
        public:
            Pipe_                       m_state;
            int                         find(std::string name);
            void                        add(std::string name);
            Pipe*                       operator[](std::string name);
            void                        connect();
            bool                        is_connected();
            void                        disconnect();
            void                        write(std::string name, std::string payload);
            void                        broadcast(std::string payload);
            pipe_msgs_t                 read(int no_of_msg = -1);
            bool                        has_message();
            PipeManager();
            ~PipeManager();
    };
}
#endif//PIPE_MANAGER_H_INCLUDED