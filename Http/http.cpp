#include "http.h"
#include "epoll.h"
#include <iostream>

//定义http响应的一些状态信息
const std::string ok_200_title = "OK";
const std::string error_400_title = "Bad Request";
const std::string error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const std::string error_403_title = "Forbidden";
const std::string error_403_form = "You do not have permission to get file form this server.\n";
const std::string error_404_title = "Not Found";
const std::string error_404_form = "The requested file was not found on this server.\n";
const std::string error_500_title = "Internal Error";
const std::string error_500_form = "There was an unusual problem serving the request file.\n";

const char* doc_path = "/home/zhang/webserver/static/html";
static epoll m_epoll; //设置为静态的，因为所有的http_con对象都共享这个epoll对象
int http_con::m_user_count = 0;
int http_con::m_epollfd = -1;

void http_con::init(int sockfd,const sockaddr_in& addr) {
    m_sockfd  = sockfd;
    m_address = addr;
    // 以下两行是为了避免TIME_WAIT状态，仅用于调试，实际使用时应该去掉
    int opt = 1;
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,(const void*)&opt,opt);//端口复用
    m_epoll.addfd(m_epollfd,sockfd,true); //注册事件
    m_user_count++; //用户数量加一
    init();
}

void http_con::close_con(bool real_close) {
    if(real_close && (m_sockfd != -1)){
        m_epoll.removefd(m_epollfd,m_sockfd); //删除事件
        m_sockfd = -1; //文件描述符置为-1
        m_user_count--; //用户数量减一
    }
}

void http_con::process(){
    http_con::HTTP_CODE read_ret = process_read(); //从缓存中读取并处理请求,并返回处理结果
    if(read_ret == NO_REQUEST) { //请求不完整，继续接受
        m_epoll.modfd(m_epollfd,m_sockfd,EPOLLIN); //注册并监听写 eopllin 读事件
        return;
    }
    if(!process_write(read_ret)) { //响应报文写入失败
        close_con(); //响应报文写入失败，关闭连接
    }
    m_epoll.modfd(m_epollfd,m_sockfd,EPOLLOUT); //注册并监听读 epollout 写事件
}

//循ead_once读取浏览器端发送来的请求报文，直到无数据可读或对方关闭连接，
//读取到m_read_buffer中，并更新m_read_idx
bool http_con::read() {
    if(m_read_idx >= READ_BUFFER_SIZE) { //退出条件，缓冲区已满
        return false;
    }
    int byte_read = 0;
    while(true) {
        //recv参数：1.文件描述符 2.接收缓冲区 3.缓冲区大小 4.接收标志
        byte_read = recv(m_sockfd,m_read_buf+m_read_idx,READ_BUFFER_SIZE-m_read_idx,0); //读取数据
        if(byte_read == -1) { //读取失败
            //非阻塞IO，EAGAIN表示暂时无数据可读
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return false;
        }else if(byte_read == 0) {
            return false;
        }
        m_read_idx += byte_read;
    }
    return true;
}

//返回结果给浏览器端
bool http_con::write() {
    int temp = 0;
    int addnew = 0;
    if(m_bytes_to_send == 0) { //没有数据要发送
        m_epoll.modfd(m_epollfd,m_sockfd,EPOLLIN); //注册并监听读事件
        init();
        return true;
    }
    while(1) {
        temp = writev(m_sockfd,m_iv,m_iv_count); //发送数据
        //writev:将多个缓冲区的数据一并写入文件描述符，参数：1.文件描述符 2.iovec结构体 3.iovec的长度
        if(temp > 0) {
            //更新已发送字节数
            m_bytes_have_send += temp;
            //偏移文件指针
            addnew = m_bytes_have_send - m_write_idx;
        }
        if(temp <= -1) {
            if(errno == EAGAIN) { //缓冲区满
                //第一个iv发完，第二个iv还没发完
                if(m_bytes_have_send >= m_iv[0].iov_len) {
                    m_iv[0].iov_len = 0;
                    m_iv[1].iov_base = m_real_file + addnew;
                    m_iv[1].iov_len = m_bytes_to_send;
                }else { //继续发送第一个
                    m_iv[0].iov_base = m_write_buf + m_bytes_to_send;
                    m_iv[0].iov_len = m_bytes_to_send;
                }
                m_epoll.modfd(m_epollfd,m_sockfd,EPOLLOUT); //注册并监听写事件
                return true;
            }
            //发送失败
            unmap(); //取消映射
            return false;
        }
        //更新已发送字节数
        m_bytes_to_send -= temp;

        //发送完成
        if(m_bytes_to_send <=  0){
            unmap();
            m_epoll.modfd(m_epollfd,m_sockfd,EPOLLIN); //注册并监听读事件
            //如果是长连接，重新初始化
            if(m_linger) {
                init();
                return true;
            }else {
                return false;
            }
        }
    }
}

sockaddr_in* http_con::get_address() {

}
//-------------------------------------------------------------------

void http_con::init(){ //数据初始化
    m_check_state = CHECK_STATE_REQUSETLINE; //主状态机初始状态
    m_request_type = GET; //默认请求类型为GET
    m_url = ""; //请求的url
    m_version = ""; //http版本号
    m_host = ""; //主机名
    m_content_length = 0; 
    m_linger = false; 

    m_start_line = 0; 
    m_checked_idx = 0; 

    m_read_idx = 0; 
    m_write_idx = 0; 

    cgi = 0; 
    m_bytes_to_send = 0; 
    m_bytes_have_send = 0; 

    memset(m_read_buf,'\0',http_con::READ_BUFFER_SIZE);
    memset(m_write_buf,'\0',http_con::WRITE_BUFFER_SIZE);
    memset(m_file_name,'\0',http_con::FILENAME_LEN);
}

http_con::HTTP_CODE http_con::process_read() { //对报文每一行循环处理
    //初始化从状态机状态，http解析结果
    LINE_STATE line_state = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* c = 0;
    std::string text;
    //判断语句
    //(若是POST请求，涉及解析消息体)&&(从状态OK) || (从状态机转移到LINE_OK，该条件涉及解析请求行和请求头部)&&(从状态OK)
    while( (m_check_state == CHEXK_STATE_CONTENT && line_state == LINE_OK) || (line_state == parse_line()) == LINE_OK ) {
        c = http_con::get_line(); // 获取当前行
        text = c; //将当前行的内容赋给text
        m_start_line = m_checked_idx; //将当前行的起始位置赋给m_start_line

        switch(m_check_state) {
            case CHECK_STATE_REQUSETLINE:{ //解析请求行
                if((ret = explain_line(text)) == BAD_REQUEST) { 
                    return BAD_REQUEST;
                }
                break;
            }
            case CHECK_STATE_HEADER:{ //解析请求头
                if((ret = explain_header(text)) == BAD_REQUEST) {
                    return BAD_REQUEST;
                }else if(ret == GET_REQUEST) {
                    return do_request(); //解析完请求头
                }
                break;
            }
            case CHEXK_STATE_CONTENT: {
                if((ret = explain_content(text)) == BAD_REQUEST) {
                    return BAD_REQUEST;
                } else if(ret == GET_REQUEST) {
                    return do_request();
                }

                line_state = LINE_OPEN; //解析完消息体，解析完成退出循环
                break;
            }
            default: {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

bool http_con::process_write(http_con::HTTP_CODE ret) {
    switch(ret){
        //内部错误 500
        case INTERNAL_ERROR:{
            //状态行
            add_state_line(500,error_500_title);
            //头部
            add_header(error_500_form.length()); //获取错误信息的长度
            if(!add_content(error_500_form)) {
                return false;
            }
            break;
        }
        //报文语法错误 404
        case BAD_REQUEST:{
            //状态行
            add_state_line(404,error_404_title);
            add_header(error_404_form.length());
            if(!add_content(error_404_form)) {
                return false;
            }
            break;
        }
        //资源没有权限 403
        case FORBIDDEN_REQUEST:{
            add_state_line(403,error_403_title);
            add_header(error_403_form.length());
            if(!add_content(error_403_form)) {
                return false;
            }
            break;
        }
        //文件存在
        case FILE_REQUEST:{
            add_state_line(200,ok_200_title);
            if(m_file_stat.st_size != 0) { //文件存在
                add_header(m_file_stat.st_size); //添加头部
                //第一个iovec指针指向m_write_buf
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                //第二个iovec指针指向mmap返回的文件指针
                m_iv[1].iov_base = m_real_file;
                m_iv[1].iov_len = m_file_stat.st_size;  
                m_iv_count = 2; //iovec的长度为2
                //发送的字节数为文件大小,响应报文的长度加上文件大小
                m_bytes_to_send = m_write_idx + m_file_stat.st_size;
                return true;
            }else {
                //返回空白html文件
                const std::string blank = "<html><body></body></html>";
                add_header(blank.length());
                if(!add_content(blank)) {
                    return false;
                }
            }
        }
        default:{
            return false;
        }
    }
    //除了文件请求，其他请求都是短连接
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

//主状态机解析请求行,获得请求方法，目标url及http版本号
http_con::HTTP_CODE http_con::explain_line(std::string text) {
    //请求行格式：GET /index.html HTTP/1.1
    //找到第一个空格或第一个\t
    int pos = text.find_first_of(" \t");
    if(pos == std::string::npos) {
        return BAD_REQUEST; //没有找到,报文格式错误
    }
    //截取请求方法 rt:request_type,获取请求方法
    std::string rt = text.substr(0,pos);
    //使用strcasecmp
    if(strcasecmp(rt.c_str(),"GET") == 0) {
        m_request_type = GET;
    }else if(strcasecmp(rt.c_str(),"POST") == 0) {
        m_request_type = POST;
        cgi = 1; //标记为cgi请求
    }else {
        return BAD_REQUEST; //不支持的请求方法
    }
    //获取url 跳过第一个空格或者\t
    int next_start = text.find_first_not_of(" \t",pos);
    if(next_start == std::string::npos) {
        return BAD_REQUEST;
    }
    
    //继续查找url和http版本号的之间的空格或\t
    int http_pos = text.find_first_of(" \t",next_start); //从next_start开始查找
    if(http_pos == std::string::npos) {
        return BAD_REQUEST;
    }

    //截取url
    m_url = text.substr(next_start,http_pos-next_start);

    //可能存在http:// 或者 https://
    if(m_url.find("http://") != std::string::npos) {
        m_url = m_url.substr(7); //去掉http://
    }
    else if(m_url.find("https://") != std::string::npos) {
        m_url = m_url.substr(8); //去掉https://
    }

    //查找url中的资源起始位
    int path_start = m_url.find("/");
    if(path_start == std::string::npos || m_url[path_start] != '/') {
        return BAD_REQUEST;
    }

    //当url为/时候，显示欢迎界面
    if(m_url.length() == path_start+1) {
        m_url += "welcome.html";
    }
    
    //提取http版本号
    pos = text.find_first_of(" \t",http_pos);
    if(pos == std::string::npos) {
        return BAD_REQUEST;
    }
    m_version = text.substr(http_pos,pos-http_pos);
    
    if(strcasecmp(m_version.c_str(),"HTTP/1.1") != 0) {
        return BAD_REQUEST;
    }
    
    //解析完请求行，转移到解析请求头部
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

//主状态机解析请求头部，获取主机名，内容长度，连接状态
http_con::HTTP_CODE http_con::explain_header(std::string text) {
    //判断是空行还是请求行，若是空行则判断是否有消息体，若有判断为POST请求
    if(text.empty()) {
        if(m_content_length != 0) { //post
            m_check_state = CHEXK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST; //直接返回
    }else if(strcasecmp(text.c_str(),"Connection:") == 0) {
        //处理connection头部
        text = text.substr(11); //跳过connection:
        if(int pos = text.find_first_of(" \t") != std::string::npos) {
            text = text.substr(0,pos);
        }
        if(strcasecmp(text.c_str(),"keep-alive") == 0) {
            m_linger = true;
        }
    }else if(strcasecmp(text.c_str(),"Content-Length:") == 0) {
        //处理conten-length
        text = text.substr(15); //跳过content-length:
        if(int pos = text.find_first_of(" \t") != std::string::npos) {
            text = text.substr(0,pos);
        }
        m_content_length = atoi(text.c_str()); //将字符串转换为整数
    }else if(strcasecmp(text.c_str(),"Host:") == 0) {
        //处理host头部
        text = text.substr(6); //跳过host:
        if(int pos = text.find_first_of(" \t") != std::string::npos) {
            text = text.substr(0,pos);
        }
        m_host = text;
    }else{
        std::cout<<"unkown header" <<text<<std::endl;
    }
}

//解析消息体
http_con::HTTP_CODE http_con::explain_content(std::string text) {
    //判断消息体是否完整
    if(m_read_idx >= (m_content_length + m_checked_idx)) {
        m_string = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}


//请求处理  ---问题待处理
http_con::HTTP_CODE http_con::do_request() {
    //设置网站根目录
    strcpy(m_file_name,doc_path);
    int len =strlen(doc_path);

    //找到m_url中的/位置
    const char* p = strrchr(m_url.c_str(),'/');

    //找到/位置
    if(cgi == 1 && (*(p+1) == '2' || *(p+1) == '3')) {
        //若cgi为1且请求资源为2或3，则启用cgi
        //...待完成
    }

    //若请求资源为0，则显示注册界面
    if(*(p+1) == '0') {
        char* url = (char*)malloc(sizeof(char)*200);
        strcpy(url,"/register.html");
        //将use_url拼接到网站根目录下，并更新m_file_name
        strncpy(m_file_name+len,url,strlen(url)); //strncpy:将url的前strlen(url)个字符拷贝到m_file_name+len
        free(url);
    }

    //若请求资源为1，则显示登录界面
    if(*(p+1) =='1') {
        char* url = (char*)malloc(sizeof(char)*200);
        strcpy(url,"/log.html");
        //将use_url拼接到网站根目录下，并更新m_file_name
        strncpy(m_file_name+len,url,strlen(url)); //strncpy:将url的前strlen(url)个字符拷贝到m_file_name+len
        free(url);
    }else {
        //将use_url拼接到网站根目录下，并更新m_file_name
        strncpy(m_file_name+len,m_url.c_str(),FILENAME_LEN-len-1);
    }

    //获取文件属性,成功将消息更新到m_file_stat结构体，失败返回NO_RESOURCE表示资源不存在
    if(stat(m_file_name,&m_file_stat) <0 ) {
        //stat函数获取文件属性，参数：1.文件名 2.文件属性结构体
        return NO_RESOURCE;
    }

    //判断文件权限
    //判断是否可读，不可读返回FORBIDDEN_REQUEST
    if(!(m_file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST; 
        //st_mode:文件权限，S_IROTH:其他用户可读
        //通过与操作判断文件是否可读
    }

    //判断是否是目录，如果是目录，返回BAD_REQUEST
    if(S_ISDIR(m_file_stat.st_mode)) {
        return BAD_REQUEST;
    }

    //以只读的方式打开
    int fd= open(m_file_name,O_RDONLY);
    //mmap函数将文件映射到内存中
    m_real_file = (char*)mmap(0,m_file_stat.st_size,PROT_READ,MAP_PRIVATE,fd,0);
    //mmap:将文件映射到内存中，参数：1.映射的起始地址 2.映射区域的长度 3.映射区域的保护方式 4.映射的标志 5.文件描述符 6.文件偏移量
    close(fd); //关闭文件描述符
    return FILE_REQUEST; //文件请求
}

//从状态机解析行
//返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
//从m_read_buf中逐字读取，判断当前是否为\r
//若是，判断下一个字符是否为\n ，将\r\n替换为\0\0,m_checked_idx指向下一行的起始位置，并返回LINE_OK
//到达末尾，返回LINE_OPEN

//若不是，判断是否是\n一般是上次读取到\r就到了buffer末尾，没有接收完整，再次接收时会出现这种情况

//既不是\r也不是\n，接受不完整，返回LINE_OPEN
http_con::LINE_STATE http_con::parse_line() { //从状态机分析出一行
    char temp;
    for(;m_checked_idx < m_read_idx;++m_checked_idx) {
        temp = m_read_buf[m_checked_idx]; //temp为当前要分析的字节
        //当前为\r
        if(temp == '\r') {
            //接受不完整
            if((m_checked_idx+1) == m_read_idx) {
                return LINE_OPEN;
            }else if(m_read_buf[m_checked_idx+1] == '\n') {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        //当前为\n
        if(temp = '\n') {
            if((m_checked_idx >1) && m_read_buf[m_checked_idx-1] == '\r') {
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    //既不是\r也不是\n，接受不完整
    return LINE_OPEN;
}

//响应行
bool http_con::add_response(const std::string& format,...) {
    if(m_write_idx >= WRITE_BUFFER_SIZE) { //超出缓存区
        return false;
    }
    va_list arg_list; //可变参数列表
    va_start(arg_list,format); //初始化可变参数列表

    //将可变参数格式化输出到缓冲区
    int len = vsnprintf(m_write_buf+m_write_idx,WRITE_BUFFER_SIZE-1-m_write_idx,format.c_str(),arg_list);
    //vsnprintf:将可变参数格式化输出到缓冲区，参数：1.缓冲区 2.缓冲区大小 3.格式化字符串 4.可变参数列表

    //如果写入的数据超出缓冲区，返回false
    if(len >= (WRITE_BUFFER_SIZE-1-m_write_idx)) {
        return false;
    }

    //更新m_write_idx
    m_write_idx += len;

    //清空可变参数列表
    va_end(arg_list);
    return true;
}

//添加消息包头，文件长度、连接状态、空行
bool http_con::add_header(int content_length){
    add_content_length(content_length); //添加content-length
    add_linger(); //添加连接状态
    add_blank_line(); //添加空行
}

//添加文本内容
bool http_con::add_content(const std::string& content) {
    return add_response("%s",content);  
}

//添加文本类型
bool http_con::add_content_type() {
    return add_response("Content-Type:%s\r\n","text/html");
}

//添加状态行
bool http_con::add_state_line(int state,const std::string& title) {
    return add_response("%s %d %s\r\n","HTTP/1.1",state,title);
}

//响应长度
bool http_con::add_content_length(int content_length) {
    return add_response("Content-Length;%d\r\n",content_length);
}

//响应连接状态
bool http_con::add_linger() {
    return add_response("Connection:%s\r\n",(m_linger == true)?"keep-alive":"close");
}

//响应空行
bool http_con::add_blank_line() {
    return add_response("%s","\r\n");
}
