#include <stdlib.h>
#include <string.h>

#include "unittest.h"

#if _HAVE_SSL_SUPPORT

#include <ape_ssl.h>

TEST(SSL, Simple)
{
	ape_ssl_t *ssl;

	ape_ssl_init();
	
	ssl = NULL;
	ssl = ape_ssl_init_global_client_ctx();
	
	EXPECT_TRUE(ssl != NULL);
	EXPECT_TRUE(ssl->ctx != NULL);
	EXPECT_TRUE(ssl->con == NULL);

	ape_ssl_destroy(ssl);
	//is here a memleak of 8 bytes?
}

/*
@TODO: ape_ssl_init_ctx
@TODO: ape_ssl_shutdown(sslCtx);
@TODO: ape_ssl_read
@TODO: ape_ssl_write
*/
#endif