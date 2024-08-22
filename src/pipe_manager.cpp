#include <pipe_manager.h>
#define PIPE_NAME TEXT("\\\\.\\pipe\\")
#define PIPE_BUFF 1024 * 16
using namespace std;
using namespace pipe_manager;
void report_error(std::string err_msg){
    DWORD err_code = GetLastError();
    string err = err_msg + "\n code(): "+std::to_string(err_msg);
    throw PipeError(err);
}
Pipe::Pipe(bool as_server, std::string name):m_server(as_server), m_name(name), m_state(Pipe_Disconnected){}
Pipe::~Pipe(){
    this->p_connect->join();
    this->p_read->join();
    CloseHandle(this->m_handle_read);
    CloseHandle(this->m_handle_write);
}
void Pipe::th_connect(){
    ConnectNamedPipe(this->m_handle_read,   NULL);
    ConnectNamedPipe(this->m_handle_write,  NULL);
    this->m_state = Pipe_Connected;
    this->p_connect = new std::thread(&Pipe::th_connect, this);
}
void Pipe::th_read(){
    while(this->m_state == Pipe_Connected){
        char buffer[PIPE_BUFF];
        memset(buffer,0,PIPE_BUFF);
        DWORD bytes_read;
        if(ReadFile(
            this->m_handle_read,
            buffer,
            PIPE_BUFF,
            &bytes_read,
            NULL) != FALSE)
        {
            std::string msg = std::string(buffer, bytes_read);
            this->m_msg_queue.push(msg);
        }
    }
    this->disconnect();
}
void Pipe::disconnect(){
    this->m_state = Pipe_Disconnecting;
    if(this->m_server){
        DisconnectNamedPipe(this->m_handle_read);
        //DisconnectNamedPipe(this->m_handle_write);
    }
    else{
        CloseHandle(this->m_handle_read);
        //CloseHandle(this->m_handle_write);
    }
    this->m_state = Pipe_Disconnected;
}
void Pipe::connect(){
    this->m_state = Pipe_Connecting;
    if(this->m_server){
        this->m_pipe_read_name = PIPE_NAME + this->m_name + "_read";
        this->m_pipe_write_name = PIPE_NAME + this->m_name + "_write";
        this->m_handle_read = CreateNamedPipe(
            this->m_pipe_read_name.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            PIPE_BUFF,
            PIPE_BUFF,
            500,
            NULL
        );
        this->m_handle_write = CreateNamedPipe(
            this->m_pipe_write_name.c_str(),
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            PIPE_BUFF,
            PIPE_BUFF,
            500,
            NULL
        );
        if(this->m_handle_read != INVALID_HANDLE_VALUE && this->m_handle_write != INVALID_HANDLE_VALUE){
            this->p_connect = new std::thread(&Pipe::th_connect, this);
        }
        else{
            report_error("Pipe Init: Fail creating failed");
        }

    }else{
        this->m_pipe_read_name = PIPE_NAME + this->m_name + "_write";
        this->m_pipe_write_name = PIPE_NAME + this->m_name + "_read";
        this->m_handle_read = CreateFile(
            this->m_pipe_read_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );
        this->m_handle_write = CreateFile(
            this->m_pipe_write_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_WRITE,
            NULL,
            OPEN_EXISTING,
            0,
            NULL
        );

        if(this->m_handle_read != INVALID_HANDLE_VALUE && this->m_handle_write != INVALID_HANDLE_VALUE){
            this->m_state = Pipe_Connected;
            this->p_read = new std::thread(&Pipe::th_read, this);
        }
        else{
            report_error("Pipe Init: Fail creating failed");
        }
    }
}
bool Pipe::is_connected(){
    return this->m_state == Pipe_Connected;
}
bool Pipe::has_message(){
    return !this->m_msg_queue.empty();
}
int Pipe::write(std::string payload){
    if(this->m_state == Pipe_Connected){
        DWORD bytes_written;
        if(WriteFile(
            this->m_handle_write,
            payload.c_str(),
            payload.length(),
            &bytes_written,
            NULL) != FALSE)
        {
            return bytes_written;
        }
        else{
            report_error("Pipe Write: Fail writing to pipe");
            return -1;
        }
    }
    return 0;
}
msgs_t Pipe::read(int no_of_msg){
    msgs_t msgs;
    if(this->m_state == Pipe_Connected){
        int q_size = this->m_msg_queue.size();
        int r_size = (no_msg == -1 || q_size < no_msg) ? q_size : no_msg;
        for(int i=0;i<r_size;i++){
            out.push_back(this->m_msg_queue.front());
            this->m_msg_queue.pop();
        }
    }
    return msgs;
}

PipeManager::PipeManager():m_state(Pipe_Disconnected){}
PipeManager::~PipeManager(){}
int PipeManager::find(std::string name){
    for(int i=0;i<this->m_pipes.size();i++){
        if(this->m_pipes[i]->m_name == name){
            return i;
        }
    }
    return -1;
}
void PipeManager::add(std::string name){
    if(this->find(name) != -1){
        throw PipeError("PipeManager::add: Pipe already exists");
    }
    this->m_pipes.push_back(new Pipe(true, name));
}
Pipe* PipeManager::operator[](std::string name){
    int index = this->find(name);
    if(index == -1){
        throw PipeError("PipeManager::operator[]: Pipe not found");
    }
    return this->m_pipes[index];
}
void PipeManager::connect(){
    this->m_state = Pipe_Connecting;
    for(int i=0;i<this->m_pipes.size();i++){
        this->m_pipes[i]->connect();
    }
}
bool PipeManager::is_connected(){
    for(int i=0;i<this->m_pipes.size();i++){
        if(!this->m_pipes[i]->is_connected()){
            return false;
        }
    }
    this->m_state = Pipe_Connected;
    return true;
}
void PipeManager::disconnect(){
    this->m_state = Pipe_Disconnecting;
    for(int i=0;i<this->m_pipes.size();i++){
        this->m_pipes[i]->disconnect();
    }
    this->m_state = Pipe_Disconnected;
}
void PipeManager::write(std::string name, std::string payload){
    int index = this->find(name);
    if(index == -1){
        throw PipeError("PipeManager::write: Pipe not found");
    }
    this->m_pipes[index]->write(payload);
}
void PipeManager::broadcast(std::string payload){
    for(int i=0;i<this->m_pipes.size();i++){
        this->m_pipes[i]->write(payload);
    }
}
pipe_msgs_t PipeManager::read(int no_of_msg){
    pipe_msgs_t msgs;
    for(int i=0;i<this->m_pipes.size();i++){
        msgs[this->m_pipes[i]->m_name] = this->m_pipes[i]->read(no_of_msg);
    }
    return msgs;
}
bool PipeManager::has_message(){
    for(int i=0;i<this->m_pipes.size();i++){
        if(this->m_pipes[i]->has_message()){
            return true;
        }
    }
    return false;
}