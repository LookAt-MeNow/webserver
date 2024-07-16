#include "http.h"
#include "epoll.h"

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

bool http_con::read() {


}

bool http_con::write() {

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
    if(rt == "GET") {
        m_request_type = GET;
    } else if(rt == "POST") {
        m_request_type = POST;
        cgi =1; //post请求标识
    }else {
        return BAD_REQUEST;
    }
    
    //获取url
    

    
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