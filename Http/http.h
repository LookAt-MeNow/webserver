#ifndef _HTTP_H_
#define _HTTP_H_

#include <netinet/in.h>
#include <string>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <cstdarg>
#include <sys/uio.h>
class http_con{
    public:
        // m_eopllfd是所有socket上的事件都注册到一个epoll内核事件表中，所以将m_epollfd设置为静态的
        static int m_epollfd;
        // 统计用户数量，因为所有的http_con对象都共享这个变量，故设置为静态
        static int m_user_count;
        // 文件名的最大长度
        static const int FILENAME_LEN = 200;
        // 读缓冲区的大小
        static const int READ_BUFFER_SIZE = 2048;
        // 写缓冲区的大小
        static const int WRITE_BUFFER_SIZE = 1024;
        // HTTP请求方法
        enum REQUEST_TYPE{
            GET, 
            POST, 
            HEAD,
            PUT,
            DELETE
        };
        // http请求的状态
        enum HTTP_CODE{
            GET_REQUEST, // 获得了一个完整的客户请求
            NO_REQUEST, // 请求不完整，需要继续读取客户数据
            INTERNAL_ERROR, // 服务器内部错误
            FILE_REQUEST, // 文件请求
            BAD_REQUEST, // 客户请求有语法错误
            NO_RESOURCE, // 没有资源
            FORBIDDEN_REQUEST, // 客户对资源没有足够的访问权限
            CLOSED_CONNECTION // 客户端已经关闭连接
        };
        
        //主状态机状态
        enum CHECK_STATE{
            CHECK_STATE_REQUSETLINE, // 请求行
            CHECK_STATE_HEADER, // 请求头
            CHEXK_STATE_CONTENT // 解析消息体，用于解析POST请求
        };
        // 从状态机状态
        enum LINE_STATE{
            LINE_OK, // 读取到一个完整的行
            LINE_BAD, // 行出错
            LINE_OPEN // 行数据尚且不完整
        };
        //--------
        //LINE_OK----(新的客户数据到达)---->LINE_OPEN
        //LINE_OPEN----(读取到回车和换行字符)---->LINE_OK
        //LINE_OPEN----(为读取到完整的请求)---->LINE_OPEN
        //LINE_OPEN----(回车和换行字符单独出现在HTTP请求中)---->LINE_BAD
        //-------

    public:
        http_con();
        ~http_con();
    
    public:
        // 初始化套接字地址
        void init(int sockfd,const sockaddr_in& addr);
        // 关闭连接
        void close_con(bool real_close = true); //默认关闭
        // 处理客户请求
        void process();
        // 读取浏览器发送的数据
        bool read();
        // 响应报文写入函数
        bool write();
        // 获取套接字
        sockaddr_in* get_address(); //
        //...数据库

    private:
        // 初始化连接数据
        void init();
        // 从缓存中读取并处理请求
        HTTP_CODE process_read();
        // 向写缓存写入相应
        bool process_write(HTTP_CODE ret);
        // 主状态机解析请求行
        HTTP_CODE explain_line(std::string text);
        // 主状态机解析请求头
        HTTP_CODE explain_header(std::string text);
        // 主状态机解析请求内容
        HTTP_CODE explain_content(std::string text);
        // 响应报文
        HTTP_CODE do_request();
        // 继续读取请求
        char* get_line() { //
            return m_read_buf+m_start_line; //返回当前正在解析的行的起始位置 
        }

        // 从状态机解析一行，判断是请求行，请求头还是请求内容
        LINE_STATE parse_line();
        //
        void unmap(); 

        //根据响应报文格式，生成对应的响应报文由do_request调用
        // 1.响应行
        bool add_response(const std::string& format,...); //参数：1.格式化字符串 2.可变参数
        // 2.响应头
        bool add_header(int content_length);
        // 3.响应内容
        bool add_content(const std::string& content);
        // 4.响应类型
        bool add_content_type();
        // 5.响应状态
        bool add_state_line(int state,const std::string& title);
        // 6.响应长度
        bool add_content_length(int content_length);
        // 7.响应连接状态
        bool add_linger();
        // 8.响应空行
        bool add_blank_line();
    private:
        int m_sockfd; // http连接中的socket
        sockaddr_in m_address; // 该http连接的地址
        char m_read_buf[READ_BUFFER_SIZE]; // 读缓冲区
        int m_read_idx; // 标记读缓冲区中已经读入的客户数据的最后一个字节的下一个位置
        
        int m_checked_idx; // 从状态机当前正在分析的字符在读缓冲区中的位置
        int m_start_line; // 每一个数据行在buf中的起始位置

        char m_write_buf[WRITE_BUFFER_SIZE]; // 写缓冲区
        int m_write_idx; // 写缓冲区中待发送的字节数

        CHECK_STATE m_check_state; // 主状态机当前所处的状态
        REQUEST_TYPE m_request_type; // 请求类型

        //对应解析请求行的数据
        char m_file_name[FILENAME_LEN]; // 客户请求的文件名
        std::string m_url; // 请求的url
        std::string m_version; // HTTP版本号
        std::string m_host; // 主机名
        int m_content_length; // 请求内容的长度
        bool m_linger; // 是否保持连接

        char* m_real_file; // 服务器上的文件的地址,映射到内存中给不适合使用string
        struct stat m_file_stat; // 文件状态
        struct iovec m_iv[2]; // 采用writev来执行写操作，定义成员变量iv来向内核注册写缓冲区
        int m_iv_count; // 写缓冲区中待发送的数据的数量
        int cgi; // 是否启用的POST
        std::string m_string; // 存储请求头数据
        int m_bytes_to_send; // 剩余发送字节数
        int m_bytes_have_send; // 已发送字节数
};
#endif