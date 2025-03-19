#include "../MemoryPool.h"
#include "http_conn.h"
#include <string>
#include <regex>
#include <fstream>

void http_conn::initmysql_result(connection_pool *connPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        // printf("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users_map[temp1] = temp2;
    }
}



// handle_auth处理客户的注册登录post请求
//m_url指的是请求内容，如/index.html、/login.html、/register.html、/2
//txt 是 user=123&passwd=123，被存入m_content_string[]中。
void http_conn::handle_post(const char* slash) 
{
                
    // printf("handle_post():.....");

    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);

    //slash指向请求的m_url的最后一个斜杠/的地址。
    //slash + 1是2：登录post，3：注册post
    if (*(slash + 1) == '2' || *(slash + 1) == '3')
    {

        
        // m_url = "/2"，text = "user=123&passwd=123"

        //根据标志判断是登录检测还是注册检测,但是flag似乎没有用到啊？？
        // char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);  //形成真实文件路径

                    // printf("post: my_real_file = %s\n", m_real_file);

        free(m_url_real);

        //将用户名和密码提取出来
        //m_content_string为：
        //user=123&passwd=123,或者是：user=111&password=123&password_confirm=123
        char name[100], password[100];
        int i;
        for (i = 5; m_content_string[i] != '&'; ++i) {
            name[i - 5] = m_content_string[i];
        }
        name[i - 5] = '\0';

        int j = 0;
        for (i = i + 10; m_content_string[i] != '\0' && m_content_string[i] != '&'; ++i, ++j) {
            password[j] = m_content_string[i];
        }
        password[j] = '\0';

        if (*(slash + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名的
            //没有重名的，进行增加数据
            //最后生成如： INSERT INTO user(username, passwd) VALUES('usr123', '123456')
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");
            
            if (!users_map.contains(name))
            {
                usr_lock.lock();
                int res = mysql_query(mysql, sql_insert);               //mysql_query() 的返回值是一个整数，通常为零表示成功执行查询，非零值表示出现了错误
                users_map.insert(pair<string, string>(name, password));
                usr_lock.unlock();

                if (!res){              // res为零表示成功
                    strcpy(m_url, "/login.html");   //strcpy是覆盖。
                }       
                    
                else {
                    strcpy(m_url, "/registerErr.html");
                }
            }
            else
                strcpy(m_url, "/registerErr.html");
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(slash + 1) == '2')
        {
            if (users_map.contains(name) && users_map[name] == password) {
                strcpy(m_url, "/index.html");
            }
            else {
                strcpy(m_url, "/loginErr.html");
            }
        }
        strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );
                        // printf("post: my_real_file = %s\n", m_real_file);
    }

}

void http_conn::handle_return(const char* slash) {


                    // printf("handle_return():.....");

    strcpy( m_real_file, doc_root );
    int len = strlen( doc_root );
    strncpy( m_real_file + len, m_url, FILENAME_LEN - len - 1 );

                    // printf("post: my_real_file = %s\n", m_real_file);

}


