#include "headers.h" //dolaczenie pliku z naglowkami

int sys_nerr, errno; //bledy
char **res=NULL; //tabica potrzebna do tokenizacji
int counter=0; //sprawdzanie ilosci parametrow
char client_hostname[64]; //host klienta

void tokenize(char *arg) // funkcja tokenizujaca
{

	char *str= arg;
	char *p=strtok(str,":"); //tokenizujemy po :
	int tok = 0;

	while(p)
	{
		res = (char **)realloc(res,sizeof(char *)* ++tok);
		if(res == NULL)
			exit(-1);
		res[tok-1] = p;
		p = strtok(NULL,":");
		counter++;
	}
	res = (char **)realloc(res, sizeof(char *)* (tok+1));
	res[tok] = 0;

	//wyswietlanie kolejnych wartosci 7666:wp.pl:80
	printf("port: %s \n", res[0]);  //7666
	printf("adres: %s \n", res[1]); //wp.pl
	printf("port: %s \n", res[2]); //80


	//w przypadku zlego uzycia wyswietl komunikat i zamnik program 
	if(counter!=3)
	{
		printf("Niepoprawna ilosc parametrow\n");
		exit(0);
	}
	counter=0;
}

int serwer_gniazdo(char *addr, int port) //funkcja odpowiedzialna za ustanowienie gniazda serwera
{
	int addrlen, s, on = 1, x;
	static struct sockaddr_in client_addr;  // podstawowa struktura sockaddr_in

	s = socket(AF_INET, SOCK_STREAM, 0); // utworzenie gniazda tcp
	if (s < 0)
		perror("socket"), exit(1); // w przypadku bledu zglos blad

	addrlen = sizeof(client_addr);
	memset(&client_addr, '\0', addrlen); //wyzerowanie reszty struktury 
	client_addr.sin_family = AF_INET; //wypelnianie sturkutry 
	client_addr.sin_addr.s_addr = inet_addr(addr); 
	client_addr.sin_port = htons(port);

	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, 4); // w razie zajecia portu wymus

	x = bind(s, (struct sockaddr *) &client_addr, addrlen); // bindowanie adresu
	if (x < 0)
		perror("bind"), exit(1); // w przypadku bledu zglas blad

	x = listen(s, 5); //nasluchiwanie
	if (x < 0)
		perror("listen"), exit(1);

	return s; // zwroc deskryptor dla gniazda
}

int otworz_host(char *host, int port) //funkcja odpowiedzialna za otworzenie hostu
{
	printf("wejscie otworz_host \n");

	struct sockaddr_in open_host; //podstawowa struktura 
	int len, s, x;
	struct hostent *H;
	int on = 1;

	H = gethostbyname(host); //gethostbyname
	if (!H)
		perror ("gethostbyname error\n"); // w razie bledu zglos blad

	len = sizeof(open_host);

	s = socket(AF_INET, SOCK_STREAM, 0); // utworz gniazdo
	if (s < 0)
	{
		perror("socket error\n");
		return s; // w razie bledu blad
	}
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &on, 4); //jezei jest zajete wymus

	len = sizeof(open_host);
	memset(&open_host, '\0', len); //zerowanie struktury

	open_host.sin_family = AF_INET; 
	memcpy(&open_host.sin_addr, H->h_addr, H->h_length);
	open_host.sin_port = htons(port); //port 

	x = connect(s, (struct sockaddr *) &open_host, len); //polacz

	if (x < 0) 
	{
		perror("connect error\n");
		close(s); //zamknij
		return x;
	}

	//set_nonblock(s);

	printf("otworz host socket, %d, x: %d\n",s,x);

	return s;
}

int sock_addr_info(struct sockaddr_in addr, int len, char *fqdn) //informacje na temat hostu po adresie
{
	struct hostent *hostinfo;

	hostinfo = gethostbyaddr((char *) &addr.sin_addr.s_addr, len, AF_INET);
	if (!hostinfo)
	{
		sprintf(fqdn, "%s", inet_ntoa(addr.sin_addr));
		return 0;
	}

	if (hostinfo && fqdn)
		sprintf(fqdn, "%s [%s]", hostinfo->h_name, inet_ntoa(addr.sin_addr));
	return 0;
}


int czekaj_na_polaczenie(int s) //wyczekiwanie polaczenia
{
	printf("wchodze do czekaj_na_polaczenie\n");

	int newsock;
	static struct sockaddr_in peer; //postawowa struktura
	socklen_t len;
	
	len = sizeof(struct sockaddr);
	
	newsock = accept(s, (struct sockaddr *) &peer, &len); //akceptowanie polaczenia
	
	if (newsock < 0) {
		if (errno != EINTR)
			perror("accept");
	}
	
	sock_addr_info(peer, len, client_hostname); //informacje 
	
	printf("przed nonblock");

	//set_nonblock(newsock);

	printf("czekaj na polaczenie, newsock: %d \n", newsock);


	return (newsock);
}

int zapis(int fd, char *buf, int *len) //zapis do gniazda
{
	int x = write(fd, buf, *len);
	if (x < 0)
		return x;
	if (x == 0)
		return x;
	if (x != *len)
		memmove(buf, buf+x, (*len)-x);
	*len -= x;
	return x;
}

void klient(int klient_d, int serwer_d) //klient, przekierowanie i funkcja select
{
	int maxfd;
	char *serwer_buf;
	char *klient_buf;
	int x, n;
	int klient_ = 0;
	int serwer_ = 0;
	fd_set T_SET;

	serwer_buf = (char *)malloc(BUF_SIZE);
	klient_buf = (char *)malloc(BUF_SIZE);

	maxfd = klient_d > serwer_d ? klient_d : serwer_d;
	maxfd++;

	while (1)
	{
		struct timeval to;
		if (klient_)
		{
			if (zapis(serwer_d, klient_buf, &klient_) < 0 && errno != EWOULDBLOCK) {
				exit(1);
			}
		}
		if (serwer_) {
			if (zapis(klient_d, serwer_buf, &serwer_) < 0 && errno != EWOULDBLOCK) {
				exit(1);
			}
		} 

		FD_ZERO(&T_SET); //wyczysc set

		if (klient_ < BUF_SIZE)
			FD_SET(klient_d, &T_SET); //dodanie zestawu
		if (serwer_ < BUF_SIZE)
			FD_SET(serwer_d, &T_SET); //dodanie zestawu

		to.tv_sec = 0;  //czekaj 0 sekund
		to.tv_usec = 1000;

		x = select(maxfd+1, &T_SET, 0, 0, 0); // obsluga deskryptorow za pomoca select

		if (x > 0)
		{
			if (FD_ISSET(klient_d, &T_SET))
			{
				n = read(klient_d, klient_buf+klient_, BUF_SIZE-klient_);
				if (n > 0) {
					klient_ += n;
				}
				else
				{
					printf("close 1\n");
					close(klient_d);
					close(serwer_d);
					exit(0);
				}
			}

			if (FD_ISSET(serwer_d, &T_SET))
			{
				n = read(serwer_d, serwer_buf+serwer_, BUF_SIZE-serwer_);
				if (n > 0)
				{
					serwer_ += n;
				}
				else
				{
					printf("close 2\n");
					close(serwer_d);
					close(klient_d);
					exit(0);
				}
			}
		}
		else if (x < 0 && errno != EINTR) {
			printf("close 3\n");
			close(serwer_d);
			close(klient_d);
			exit(0);
		}
	}
}


int main(int argc, char *argv[])
{
	char *localaddr = (char *)"127.0.0.1"; //adres lokalny 
	int client, server;
	int master_sock;   
	int k=0;

	if(argc > 1)
	{    

		for(k=1; k<argc; k++)
		{
			tokenize(argv[k]);

			int localport = atoi(res[0]); //wyniki z tokenize
			char *remoteaddr = (char *)(res[1]); //wyniki z tokenize
			int remoteport = atoi(res[2]); //wyniki z tokenize
			master_sock = serwer_gniazdo(localaddr, localport);
 			
			if(!fork())
			{
				int potomek=0, status=0;
				client = czekaj_na_polaczenie(master_sock);	
						server = otworz_host(remoteaddr, remoteport);					

				//if (!(potomek=fork()))
			//	{   //POTOMEK

					printf("przed czekaj_na_polaczenie\n");					
					klient(client, server);
					printf("za klientem\n");					
					close(client);
				    //close(server);	           
					//close(master_sock);					
			//	}
			//	waitpid(potomek, &status,0);

			}
		}
	}
	else
	{
		printf("Uzycie: port:adres:port");
	}
	return 0;
}
