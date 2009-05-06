#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#include <Eina.h>
#include <eupnp.h>
#include <eupnp_ssdp.h>
#include <eupnp_control_point.h>

int sock;
int exit_req = 0;

void terminate(int p)
{
   exit_req = 1;
}

/*
 * Sends a test search and listens for datagrams. Tests MSearch and SSDP
 * server.
 *
 * Run with "EINA_ERROR_LEVEL=3 ./eupnp_basic_control_point" for watching debug
 * messages.
 */
int main(void)
{
   signal(SIGTERM, terminate);
   signal(SIGINT, terminate);
   eupnp_init();

   int ret;
   fd_set r, w, ex;
   Eupnp_Control_Point *c;

   c = eupnp_control_point_new();

   if (!c)
     {
	eupnp_shutdown();
	return -1;
     }

   /* Send a test search */
   if (!eupnp_control_point_discovery_request_send(c, 5, "ssdp:all"))
     {
	EINA_ERROR_PWARN("Failed to perform MSearch.\n");
     }
    else
	EINA_ERROR_PDBG("MSearch sent sucessfully.\n");

   sock = c->ssdp_server->udp_sock->socket;

   while (!exit_req)
     {
	FD_ZERO(&r);
	FD_ZERO(&w);
	FD_ZERO(&ex);
	FD_SET(sock, &r);
	ret = select(sock+1, &r, NULL, NULL, NULL);

	if (ret < 0)
	  {
	     perror("Select error");
	     break;
	  }

	if (exit_req)
	   break;

	if (FD_ISSET(sock, &r))
	  { 
	       /* This is the event handler that will be added to the event loop.
	       * For example, when integrating eupnp+ecore, this socket will
	       * internally be added with ecore_fd_handler_add and the handler
	       * passed will be a wrapper of _eupnp_ssdp_on_datagram_available.
	       */
	      _eupnp_ssdp_on_datagram_available(c->ssdp_server);
          }
     }

   eupnp_control_point_free(c);
   eupnp_shutdown();
   return 0;
}
