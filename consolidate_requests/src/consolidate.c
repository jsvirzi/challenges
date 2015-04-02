#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <syslog.h>
#include <errno.h>
#include <poll.h>
#include <sys/ioctl.h>

#include <string>
#include <vector>

std::string parse(const char *input, const char *field);

std::string parse_str(const std::string &s, const std::string &key) {
	return parse(s.c_str(), key.c_str());
};

int parse_int(const std::string &s, const std::string key) {
	std::string tmp = parse(s.c_str(), key.c_str());
	return atoi(tmp.c_str());
};

class HttpRequestParser {
	public:
	HttpRequestParser(const char *str, int msg_len);
	HttpRequestParser(std::string &str);
	~HttpRequestParser();
	int get_content_length();
	int get_content_length(const char *msg, int msg_len);
	unsigned char *get_content(unsigned char *p = 0, int max_len = 0);
	private:
	void setup(const char *str, int msg_len);
	char *buff;
	unsigned char *content;
	int content_length, buff_length;
	std::vector<std::string> lines;
};

std::string form_output_command(std::string &url, std::string &key, int amount) {
	std::string res, payload;
#define STRSIZE 1024
	int str_size = STRSIZE;
	char str[STRSIZE];
	sprintf(str, "POST / HTTP/1.1\r\n");
	res += str;
/* if we need to add agent */
	// sprintf(str, "User-Agent: curl/7.35.0");
	// res += str;
	sprintf(str, "Host: %s\r\n", url.c_str()); 
	res += str;
	sprintf(str, "Accept: */*\r\n");
	res += str;
/* example: key=clicks-b&amount=10 */
	sprintf(str, "key=%s&amount=%d", key.c_str(), amount);
	payload = str;
	sprintf(str, "Content-Length: %d\r\n", payload.length());
	res += str;
	sprintf(str, "Content-Type: application/x-www-form-urlencoded\r\n\r\n");
	res += str;
	res += payload;
	return res;
}

std::string form_http_response() {
	std::string response;
	response += "HTTP/1.0 200 OK\n";
	// response += "Date: Fri, 31 Dec 1999 23:59:59 GMT\n";
	response += "Content-Type: text/html\n";
	response += "Content-Length: 0\n";
	response += "Connection: close\n\n";
	return response;
}

/*----------------------------------------------------------------------
 Portable function to set a socket into nonblocking mode.
 Calling this on a socket causes all future read() and write() calls on
 that socket to do only as much as they can immediately, and return 
 without waiting.
 If no data can be read or written, they return -1 and set errno
 to EAGAIN (or EWOULDBLOCK).
 Thanks to Bjorn Reese for this code.
----------------------------------------------------------------------*/
int setNonblocking(int fd)
{
    int flags;

/* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
printf("O_NONBLOCK is defined as %x\n", O_NONBLOCK);
/* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
if (-1 == (flags = fcntl(fd, F_GETFL, 0)))
flags = 0;
return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
/* Otherwise, use the old way of doing it */
flags = 1;
return ioctl(fd, FIOBIO, &flags);
#endif
}     

bool run = true;
static void stop (int sig) { run = false; }

#define MAXCLIENTS 10
main() {
	int recv_port, xmit_port;
	struct sockaddr_in servaddr;

	std::string url = "127.0.0.1:8000";

	recv_port = 8001;
	xmit_port = 8000;

/* listen port */
	int recv_fd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port   = htons(recv_port);
	bind(recv_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	listen(recv_fd,5);
	setNonblocking(recv_fd); /* socket into nonblocking mode */

/* transmit port */
	int xmit_fd = socket(AF_INET, SOCK_STREAM, 0);
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port   = htons(xmit_port);
	bind(xmit_fd, (struct sockaddr *) &servaddr, sizeof(servaddr));
	setNonblocking(xmit_fd); /* socket into nonblocking mode */

/* exception handling */
	signal(SIGINT, stop); /* ^C */
	signal(SIGUSR1, stop);

	while(run) {
		int fd = accept(recv_fd, NULL, NULL);
		setNonblocking(fd);
		if(fd != -1) {
#define BUFSIZE 1024
			char buf[BUFSIZE];
			bool loop = true;
			while(loop) {
				int n = read(fd, buf, BUFSIZE);
				if(n > 0) {
					printf("read %d bytes from socket\n", n);
					buf[n] = 0;
					printf("message = [%s]\n", buf);
					HttpRequestParser parser(buf, n);
					printf("content length = %d\n", parser.get_content_length());
					std::string content = (char *)parser.get_content();
					printf("content = [%s]\n", content.c_str());
					std::string key = parse_str(content, std::string("key"));
					int amount = parse_int(content, std::string("amount"));
printf("key = %s. amount = %d\n", key.c_str(), amount);
					std::string command = form_output_command(url, key, amount);
printf("command = [%s]\n", command.c_str());
#if 0
					for(int i=0;i<(n-1);++i) {
printf("byte %d = '%c' = %d = %x\n", i, buf[i], buf[i], buf[i]); 
						if(buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n' ) {
							printf("payload = %s\n", &buf[i+4]);
							break;
						}
					}
#endif
				} else if(n == 0) {
					printf("message completed\n");
					close(fd);
					loop = false;
				} else { printf("read() returns %d\n", n);
				}
				// sleep(1);
				std::string response = form_http_response();
				write(fd, response.c_str(), response.length());
				close(fd);
				loop = false;
				// printf("strlen = %d\n", strlen(html));
			}
		}
		
		sleep(1);
	}
	printf("goodbye!\n");
	close(recv_fd);
	close(xmit_fd);
	return 0;
}

void HttpRequestParser::setup(const char *str, int msg_len) {
	content = 0;
	int i, j;
	buff_length = msg_len + 1;
	buff = new char [ buff_length ]; /* handle null termination */
	memcpy(buff, str, msg_len);
	buff[msg_len] = 0; /* null-terminate if not already so */
	get_content_length();
	if(content_length > 0) {
		content = new unsigned char [ content_length ];
		unsigned char cha = buff[0], chb = buff[1], chc = buff[2], chd;
		for(i=3;i<msg_len;++i) {
			chd = buff[i];
			if(cha == '\r' && chb == '\n' && chc == '\r' && chd == '\n') {
				for(j=0;j<content_length;++j) content[j] = buff[i+1+j];
				break;
			}
			cha = chb; chb = chc; chc = chd; /* comb filter */
		}
	}

#if 0
	for(i=0;i<msg_len;++i) {
		ch = str[i];
		buff[i] = (ch == 0xa || ch == 0xd) ? 0 : ch; /* replace '\n' and '\r' with 0 */
	} 
	for(i=0;i<msg_len;++i)
#endif

}

HttpRequestParser::HttpRequestParser(const char *str, int msg_len) {
	setup(str, msg_len);
}

HttpRequestParser::HttpRequestParser(std::string &str) {
	setup(str.c_str(), str.length());
}

HttpRequestParser::~HttpRequestParser() {
	if(buff) delete [] buff;
	if(content) delete [] content;
}

int HttpRequestParser::get_content_length() {
	return get_content_length(buff, buff_length);
}

int HttpRequestParser::get_content_length(const char *msg, int msg_len) {
	bool found = false;
	const char *field = "Content-Length:";
	int i, j, k, field_len = strlen(field);
	content_length = -1;
	for(i=0;i<(msg_len - field_len);++i) {
		if(found) break;
		if(strncmp(&msg[i], field, field_len)) continue;
		for(j=i+field_len;j<msg_len;++j) {
			if(msg[j] == 0xa || msg[j] == 0xd) {
				int line_length = j - i - field_len;
#define TMPBUFSIZE 256
				char tmp_str [ TMPBUFSIZE ]; /* more than ample */
				if(line_length > TMPBUFSIZE) return -1;
				for(k=0;k<line_length;++k) tmp_str[k] = msg[i+field_len+k];
				tmp_str[k] = 0; /* null terminate */
				content_length = atoi(tmp_str);
printf("content_length = %d\n", content_length);
				found = true;
				break;
			}
		}
	}
	return content_length;
}

unsigned char *HttpRequestParser::get_content(unsigned char *p, int max_len) {
	if(p && (content_length <= max_len)) memcpy(p, content, content_length);
	else if(p) return 0;
	return content;
}

/*** parse ***/

std::string parse(const char *input, const char *field) {
	int i, n, len1 = strlen(input), len2 = strlen(field);
	std::string result;
	for(i=0;i<(len1-len2);++i) {
		if((strncmp(&input[i], field, len2) == 0) && (input[i+len2] == '=')) {
			int begin = i + len2 + 1;
			int final = begin;
			for(i=begin;i<len1;++i) { /* search for next delimiter */
				if(input[i] == '&') {
					final = i;
					char *str = new char [ final - begin + 1 ];
					for(i=begin;i<final;++i) {
						str[i-begin] = input[i];
					}
					str[i-begin] = 0;
					result = str;
					delete [] str;
					return result;
				}
			}
		/* delimiter not found. the actual field goes to end of string */
			final = len1;
			char *str = new char [ final - begin + 1 ];
			for(i=begin;i<final;++i) {
				str[i-begin] = input[i];
			}
			str[i-begin] = 0;
			result = str;
			delete [] str;
			return result;
		} 
	}
	return result;
}


