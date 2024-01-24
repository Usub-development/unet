//
// Created by Kirill Zhukov on 31.10.2023.
//

#ifndef TCPSOCKETTEST_CTX_UTIL_H
#define TCPSOCKETTEST_CTX_UTIL_H

#ifdef __linux__
#include <cstdlib>
#endif
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "logging/logger.h"
#include "nghttp2/nghttp2.h"

constexpr char DEFAULT_CIPHER_LIST[] =
        "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-"
        "AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-"
        "POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-"
        "AES256-GCM-SHA384";

constexpr unsigned char ALPN_COMBINED[] = {
        2, 'h', '2',
        5, 'h', '2', '-', '1', '6',
        5, 'h', '2', '-', '1', '4'
};

#if OPENSSL_VERSION_NUMBER >= 0x10002000L

static int alpn_select_proto_cb(SSL *ssl, const unsigned char **out,
                                unsigned char *outlen, const unsigned char *in,
                                unsigned int inlen, void *arg) {
    int rv;
    (void) ssl;
    (void) arg;

    rv = nghttp2_select_next_protocol((unsigned char **) out, outlen, in, inlen);

    if (rv != 1) {
        return SSL_TLSEXT_ERR_NOACK;
    }

    return SSL_TLSEXT_ERR_OK;
}

#endif /* OPENSSL_VERSION_NUMBER >= 0x10002000L */

static SSL_CTX* create_ssl_ctx(const char *key_file, const char *cert_file) {
    SSL_CTX *ssl_ctx;
    ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) {
        error_log("Could not create SSL/TLS context: %s", ERR_error_string(ERR_get_error(), nullptr));
    }
    SSL_CTX_set_options(ssl_ctx,
                        SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                        SSL_OP_NO_COMPRESSION |
                        SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
    if (SSL_CTX_set1_curves_list(ssl_ctx, "P-256") != 1) {
        error_log("SSL_CTX_set1_curves_list failed: %s", ERR_error_string(ERR_get_error(), nullptr));
    }
#else  /* !(OPENSSL_VERSION_NUMBER >= 0x30000000L) */
    {
        EC_KEY *ecdh;
        ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
        if (!ecdh) {
            errx(1, "EC_KEY_new_by_curv_name failed: %s",
                 ERR_error_string(ERR_get_error(), NULL));
        }
        SSL_CTX_set_tmp_ecdh(ssl_ctx, ecdh);
        EC_KEY_free(ecdh);
    }
#endif /* !(OPENSSL_VERSION_NUMBER >= 0x30000000L) */

    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) != 1) {
        error_log("Could not read private key file %s", key_file);
    }
    if (SSL_CTX_use_certificate_chain_file(ssl_ctx, cert_file) != 1) {
        error_log("Could not read certificate file %s", cert_file);
    }


#ifndef OPENSSL_NO_NEXTPROTONEG
    SSL_CTX_set_next_protos_advertised_cb(
            ssl_ctx,
            [](SSL *s, const unsigned char **data, unsigned int *len, void *arg) {
                *data = ALPN_COMBINED;
                *len = sizeof(ALPN_COMBINED);
                return SSL_TLSEXT_ERR_OK;
            },
            nullptr);
#endif // !OPENSSL_NO_NEXTPROTONEG

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
    SSL_CTX_set_alpn_select_cb(ssl_ctx, alpn_select_proto_cb, NULL);
#endif /* OPENSSL_VERSION_NUMBER >= 0x10002000L */

    return ssl_ctx;
}

static SSL_CTX* create_http1_ctx(const char *key_file, const char *cert_file) {
    SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
    SSL_CTX_use_certificate_file(ctx, cert_file, SSL_FILETYPE_PEM);
    SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM);
    return ctx;
}

static SSL *create_ssl(SSL_CTX *ssl_ctx) {
    SSL *ssl;
    ssl = SSL_new(ssl_ctx);
    if (!ssl) {
        error_log("Could not create SSL/TLS session object: %s",
             ERR_error_string(ERR_get_error(), nullptr));
    }
    return ssl;
}

#endif //TCPSOCKETTEST_CTX_UTIL_H
