cmake_minimum_required(VERSION 3.29)
project(volcano C)

set(CMAKE_C_STANDARD 11)

add_compile_options(-Wall -Wextra -Wpedantic -g3 -fpic -fpie -fPIE)
#link_libraries(-lz)

set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-O1)
  add_compile_definitions(DEBUG)
else ()
  add_compile_options(-O2)
  add_compile_definitions(NDEBUG)
endif ()

include_directories(. third-party/)


add_executable(volcanotest
    examples/runtests.c
)

add_executable(volcano examples/volcano_example.c )



add_library(bearssl STATIC
    #third-party/BearSSL/samples/client_basic.c
    #third-party/BearSSL/samples/custom_profile.c
    #third-party/BearSSL/samples/server_basic.c
    third-party/BearSSL/src/aead/ccm.c
    third-party/BearSSL/src/aead/eax.c
    third-party/BearSSL/src/aead/gcm.c
    third-party/BearSSL/src/codec/ccopy.c
    third-party/BearSSL/src/codec/dec16be.c
    third-party/BearSSL/src/codec/dec16le.c
    third-party/BearSSL/src/codec/dec32be.c
    third-party/BearSSL/src/codec/dec32le.c
    third-party/BearSSL/src/codec/dec64be.c
    third-party/BearSSL/src/codec/dec64le.c
    third-party/BearSSL/src/codec/enc16be.c
    third-party/BearSSL/src/codec/enc16le.c
    third-party/BearSSL/src/codec/enc32be.c
    third-party/BearSSL/src/codec/enc32le.c
    third-party/BearSSL/src/codec/enc64be.c
    third-party/BearSSL/src/codec/enc64le.c
    third-party/BearSSL/src/codec/pemdec.c
    third-party/BearSSL/src/codec/pemenc.c
    third-party/BearSSL/src/ec/ecdsa_atr.c
    third-party/BearSSL/src/ec/ecdsa_default_sign_asn1.c
    third-party/BearSSL/src/ec/ecdsa_default_sign_raw.c
    third-party/BearSSL/src/ec/ecdsa_default_vrfy_asn1.c
    third-party/BearSSL/src/ec/ecdsa_default_vrfy_raw.c
    third-party/BearSSL/src/ec/ecdsa_i15_bits.c
    third-party/BearSSL/src/ec/ecdsa_i15_sign_asn1.c
    third-party/BearSSL/src/ec/ecdsa_i15_sign_raw.c
    third-party/BearSSL/src/ec/ecdsa_i15_vrfy_asn1.c
    third-party/BearSSL/src/ec/ecdsa_i15_vrfy_raw.c
    third-party/BearSSL/src/ec/ecdsa_i31_bits.c
    third-party/BearSSL/src/ec/ecdsa_i31_sign_asn1.c
    third-party/BearSSL/src/ec/ecdsa_i31_sign_raw.c
    third-party/BearSSL/src/ec/ecdsa_i31_vrfy_asn1.c
    third-party/BearSSL/src/ec/ecdsa_i31_vrfy_raw.c
    third-party/BearSSL/src/ec/ecdsa_rta.c
    third-party/BearSSL/src/ec/ec_all_m15.c
    third-party/BearSSL/src/ec/ec_all_m31.c
    third-party/BearSSL/src/ec/ec_c25519_i15.c
    third-party/BearSSL/src/ec/ec_c25519_i31.c
    third-party/BearSSL/src/ec/ec_c25519_m15.c
    third-party/BearSSL/src/ec/ec_c25519_m31.c
    third-party/BearSSL/src/ec/ec_c25519_m62.c
    third-party/BearSSL/src/ec/ec_c25519_m64.c
    third-party/BearSSL/src/ec/ec_curve25519.c
    third-party/BearSSL/src/ec/ec_default.c
    third-party/BearSSL/src/ec/ec_keygen.c
    third-party/BearSSL/src/ec/ec_p256_m15.c
    third-party/BearSSL/src/ec/ec_p256_m31.c
    third-party/BearSSL/src/ec/ec_p256_m62.c
    third-party/BearSSL/src/ec/ec_p256_m64.c
    third-party/BearSSL/src/ec/ec_prime_i15.c
    third-party/BearSSL/src/ec/ec_prime_i31.c
    third-party/BearSSL/src/ec/ec_pubkey.c
    third-party/BearSSL/src/ec/ec_secp256r1.c
    third-party/BearSSL/src/ec/ec_secp384r1.c
    third-party/BearSSL/src/ec/ec_secp521r1.c
    third-party/BearSSL/src/hash/dig_oid.c
    third-party/BearSSL/src/hash/dig_size.c
    third-party/BearSSL/src/hash/ghash_ctmul.c
    third-party/BearSSL/src/hash/ghash_ctmul32.c
    third-party/BearSSL/src/hash/ghash_ctmul64.c
    third-party/BearSSL/src/hash/ghash_pclmul.c
    third-party/BearSSL/src/hash/ghash_pwr8.c
    third-party/BearSSL/src/hash/md5.c
    third-party/BearSSL/src/hash/md5sha1.c
    third-party/BearSSL/src/hash/mgf1.c
    third-party/BearSSL/src/hash/multihash.c
    third-party/BearSSL/src/hash/sha1.c
    third-party/BearSSL/src/hash/sha2big.c
    third-party/BearSSL/src/hash/sha2small.c
    third-party/BearSSL/src/int/i15_add.c
    third-party/BearSSL/src/int/i15_bitlen.c
    third-party/BearSSL/src/int/i15_decmod.c
    third-party/BearSSL/src/int/i15_decode.c
    third-party/BearSSL/src/int/i15_decred.c
    third-party/BearSSL/src/int/i15_encode.c
    third-party/BearSSL/src/int/i15_fmont.c
    third-party/BearSSL/src/int/i15_iszero.c
    third-party/BearSSL/src/int/i15_moddiv.c
    third-party/BearSSL/src/int/i15_modpow.c
    third-party/BearSSL/src/int/i15_modpow2.c
    third-party/BearSSL/src/int/i15_montmul.c
    third-party/BearSSL/src/int/i15_mulacc.c
    third-party/BearSSL/src/int/i15_muladd.c
    third-party/BearSSL/src/int/i15_ninv15.c
    third-party/BearSSL/src/int/i15_reduce.c
    third-party/BearSSL/src/int/i15_rshift.c
    third-party/BearSSL/src/int/i15_sub.c
    third-party/BearSSL/src/int/i15_tmont.c
    third-party/BearSSL/src/int/i31_add.c
    third-party/BearSSL/src/int/i31_bitlen.c
    third-party/BearSSL/src/int/i31_decmod.c
    third-party/BearSSL/src/int/i31_decode.c
    third-party/BearSSL/src/int/i31_decred.c
    third-party/BearSSL/src/int/i31_encode.c
    third-party/BearSSL/src/int/i31_fmont.c
    third-party/BearSSL/src/int/i31_iszero.c
    third-party/BearSSL/src/int/i31_moddiv.c
    third-party/BearSSL/src/int/i31_modpow.c
    third-party/BearSSL/src/int/i31_modpow2.c
    third-party/BearSSL/src/int/i31_montmul.c
    third-party/BearSSL/src/int/i31_mulacc.c
    third-party/BearSSL/src/int/i31_muladd.c
    third-party/BearSSL/src/int/i31_ninv31.c
    third-party/BearSSL/src/int/i31_reduce.c
    third-party/BearSSL/src/int/i31_rshift.c
    third-party/BearSSL/src/int/i31_sub.c
    third-party/BearSSL/src/int/i31_tmont.c
    third-party/BearSSL/src/int/i32_add.c
    third-party/BearSSL/src/int/i32_bitlen.c
    third-party/BearSSL/src/int/i32_decmod.c
    third-party/BearSSL/src/int/i32_decode.c
    third-party/BearSSL/src/int/i32_decred.c
    third-party/BearSSL/src/int/i32_div32.c
    third-party/BearSSL/src/int/i32_encode.c
    third-party/BearSSL/src/int/i32_fmont.c
    third-party/BearSSL/src/int/i32_iszero.c
    third-party/BearSSL/src/int/i32_modpow.c
    third-party/BearSSL/src/int/i32_montmul.c
    third-party/BearSSL/src/int/i32_mulacc.c
    third-party/BearSSL/src/int/i32_muladd.c
    third-party/BearSSL/src/int/i32_ninv32.c
    third-party/BearSSL/src/int/i32_reduce.c
    third-party/BearSSL/src/int/i32_sub.c
    third-party/BearSSL/src/int/i32_tmont.c
    third-party/BearSSL/src/int/i62_modpow2.c
    third-party/BearSSL/src/kdf/hkdf.c
    third-party/BearSSL/src/kdf/shake.c
    third-party/BearSSL/src/mac/hmac.c
    third-party/BearSSL/src/mac/hmac_ct.c
    third-party/BearSSL/src/rand/aesctr_drbg.c
    third-party/BearSSL/src/rand/hmac_drbg.c
    third-party/BearSSL/src/rand/sysrng.c
    third-party/BearSSL/src/rsa/rsa_default_keygen.c
    third-party/BearSSL/src/rsa/rsa_default_modulus.c
    third-party/BearSSL/src/rsa/rsa_default_oaep_decrypt.c
    third-party/BearSSL/src/rsa/rsa_default_oaep_encrypt.c
    third-party/BearSSL/src/rsa/rsa_default_pkcs1_sign.c
    third-party/BearSSL/src/rsa/rsa_default_pkcs1_vrfy.c
    third-party/BearSSL/src/rsa/rsa_default_priv.c
    third-party/BearSSL/src/rsa/rsa_default_privexp.c
    third-party/BearSSL/src/rsa/rsa_default_pss_sign.c
    third-party/BearSSL/src/rsa/rsa_default_pss_vrfy.c
    third-party/BearSSL/src/rsa/rsa_default_pub.c
    third-party/BearSSL/src/rsa/rsa_default_pubexp.c
    third-party/BearSSL/src/rsa/rsa_i15_keygen.c
    third-party/BearSSL/src/rsa/rsa_i15_modulus.c
    third-party/BearSSL/src/rsa/rsa_i15_oaep_decrypt.c
    third-party/BearSSL/src/rsa/rsa_i15_oaep_encrypt.c
    third-party/BearSSL/src/rsa/rsa_i15_pkcs1_sign.c
    third-party/BearSSL/src/rsa/rsa_i15_pkcs1_vrfy.c
    third-party/BearSSL/src/rsa/rsa_i15_priv.c
    third-party/BearSSL/src/rsa/rsa_i15_privexp.c
    third-party/BearSSL/src/rsa/rsa_i15_pss_sign.c
    third-party/BearSSL/src/rsa/rsa_i15_pss_vrfy.c
    third-party/BearSSL/src/rsa/rsa_i15_pub.c
    third-party/BearSSL/src/rsa/rsa_i15_pubexp.c
    third-party/BearSSL/src/rsa/rsa_i31_keygen.c
    third-party/BearSSL/src/rsa/rsa_i31_keygen_inner.c
    third-party/BearSSL/src/rsa/rsa_i31_modulus.c
    third-party/BearSSL/src/rsa/rsa_i31_oaep_decrypt.c
    third-party/BearSSL/src/rsa/rsa_i31_oaep_encrypt.c
    third-party/BearSSL/src/rsa/rsa_i31_pkcs1_sign.c
    third-party/BearSSL/src/rsa/rsa_i31_pkcs1_vrfy.c
    third-party/BearSSL/src/rsa/rsa_i31_priv.c
    third-party/BearSSL/src/rsa/rsa_i31_privexp.c
    third-party/BearSSL/src/rsa/rsa_i31_pss_sign.c
    third-party/BearSSL/src/rsa/rsa_i31_pss_vrfy.c
    third-party/BearSSL/src/rsa/rsa_i31_pub.c
    third-party/BearSSL/src/rsa/rsa_i31_pubexp.c
    third-party/BearSSL/src/rsa/rsa_i32_oaep_decrypt.c
    third-party/BearSSL/src/rsa/rsa_i32_oaep_encrypt.c
    third-party/BearSSL/src/rsa/rsa_i32_pkcs1_sign.c
    third-party/BearSSL/src/rsa/rsa_i32_pkcs1_vrfy.c
    third-party/BearSSL/src/rsa/rsa_i32_priv.c
    third-party/BearSSL/src/rsa/rsa_i32_pss_sign.c
    third-party/BearSSL/src/rsa/rsa_i32_pss_vrfy.c
    third-party/BearSSL/src/rsa/rsa_i32_pub.c
    third-party/BearSSL/src/rsa/rsa_i62_keygen.c
    third-party/BearSSL/src/rsa/rsa_i62_oaep_decrypt.c
    third-party/BearSSL/src/rsa/rsa_i62_oaep_encrypt.c
    third-party/BearSSL/src/rsa/rsa_i62_pkcs1_sign.c
    third-party/BearSSL/src/rsa/rsa_i62_pkcs1_vrfy.c
    third-party/BearSSL/src/rsa/rsa_i62_priv.c
    third-party/BearSSL/src/rsa/rsa_i62_pss_sign.c
    third-party/BearSSL/src/rsa/rsa_i62_pss_vrfy.c
    third-party/BearSSL/src/rsa/rsa_i62_pub.c
    third-party/BearSSL/src/rsa/rsa_oaep_pad.c
    third-party/BearSSL/src/rsa/rsa_oaep_unpad.c
    third-party/BearSSL/src/rsa/rsa_pkcs1_sig_pad.c
    third-party/BearSSL/src/rsa/rsa_pkcs1_sig_unpad.c
    third-party/BearSSL/src/rsa/rsa_pss_sig_pad.c
    third-party/BearSSL/src/rsa/rsa_pss_sig_unpad.c
    third-party/BearSSL/src/rsa/rsa_ssl_decrypt.c
    third-party/BearSSL/src/settings.c
    third-party/BearSSL/src/ssl/prf.c
    third-party/BearSSL/src/ssl/prf_md5sha1.c
    third-party/BearSSL/src/ssl/prf_sha256.c
    third-party/BearSSL/src/ssl/prf_sha384.c
    third-party/BearSSL/src/ssl/ssl_ccert_single_ec.c
    third-party/BearSSL/src/ssl/ssl_ccert_single_rsa.c
    third-party/BearSSL/src/ssl/ssl_client.c
    third-party/BearSSL/src/ssl/ssl_client_default_rsapub.c
    third-party/BearSSL/src/ssl/ssl_client_full.c
    third-party/BearSSL/src/ssl/ssl_engine.c
    third-party/BearSSL/src/ssl/ssl_engine_default_aescbc.c
    third-party/BearSSL/src/ssl/ssl_engine_default_aesccm.c
    third-party/BearSSL/src/ssl/ssl_engine_default_aesgcm.c
    third-party/BearSSL/src/ssl/ssl_engine_default_chapol.c
    third-party/BearSSL/src/ssl/ssl_engine_default_descbc.c
    third-party/BearSSL/src/ssl/ssl_engine_default_ec.c
    third-party/BearSSL/src/ssl/ssl_engine_default_ecdsa.c
    third-party/BearSSL/src/ssl/ssl_engine_default_rsavrfy.c
    third-party/BearSSL/src/ssl/ssl_hashes.c
    third-party/BearSSL/src/ssl/ssl_hs_client.c
    third-party/BearSSL/src/ssl/ssl_hs_server.c
    third-party/BearSSL/src/ssl/ssl_io.c
    third-party/BearSSL/src/ssl/ssl_keyexport.c
    third-party/BearSSL/src/ssl/ssl_lru.c
    third-party/BearSSL/src/ssl/ssl_rec_cbc.c
    third-party/BearSSL/src/ssl/ssl_rec_ccm.c
    third-party/BearSSL/src/ssl/ssl_rec_chapol.c
    third-party/BearSSL/src/ssl/ssl_rec_gcm.c
    third-party/BearSSL/src/ssl/ssl_scert_single_ec.c
    third-party/BearSSL/src/ssl/ssl_scert_single_rsa.c
    third-party/BearSSL/src/ssl/ssl_server.c
    third-party/BearSSL/src/ssl/ssl_server_full_ec.c
    third-party/BearSSL/src/ssl/ssl_server_full_rsa.c
    third-party/BearSSL/src/ssl/ssl_server_mine2c.c
    third-party/BearSSL/src/ssl/ssl_server_mine2g.c
    third-party/BearSSL/src/ssl/ssl_server_minf2c.c
    third-party/BearSSL/src/ssl/ssl_server_minf2g.c
    third-party/BearSSL/src/ssl/ssl_server_minr2g.c
    third-party/BearSSL/src/ssl/ssl_server_minu2g.c
    third-party/BearSSL/src/ssl/ssl_server_minv2g.c
    third-party/BearSSL/src/symcipher/aes_big_cbcdec.c
    third-party/BearSSL/src/symcipher/aes_big_cbcenc.c
    third-party/BearSSL/src/symcipher/aes_big_ctr.c
    third-party/BearSSL/src/symcipher/aes_big_ctrcbc.c
    third-party/BearSSL/src/symcipher/aes_big_dec.c
    third-party/BearSSL/src/symcipher/aes_big_enc.c
    third-party/BearSSL/src/symcipher/aes_common.c
    third-party/BearSSL/src/symcipher/aes_ct.c
    third-party/BearSSL/src/symcipher/aes_ct64.c
    third-party/BearSSL/src/symcipher/aes_ct64_cbcdec.c
    third-party/BearSSL/src/symcipher/aes_ct64_cbcenc.c
    third-party/BearSSL/src/symcipher/aes_ct64_ctr.c
    third-party/BearSSL/src/symcipher/aes_ct64_ctrcbc.c
    third-party/BearSSL/src/symcipher/aes_ct64_dec.c
    third-party/BearSSL/src/symcipher/aes_ct64_enc.c
    third-party/BearSSL/src/symcipher/aes_ct_cbcdec.c
    third-party/BearSSL/src/symcipher/aes_ct_cbcenc.c
    third-party/BearSSL/src/symcipher/aes_ct_ctr.c
    third-party/BearSSL/src/symcipher/aes_ct_ctrcbc.c
    third-party/BearSSL/src/symcipher/aes_ct_dec.c
    third-party/BearSSL/src/symcipher/aes_ct_enc.c
    third-party/BearSSL/src/symcipher/aes_pwr8.c
    third-party/BearSSL/src/symcipher/aes_pwr8_cbcdec.c
    third-party/BearSSL/src/symcipher/aes_pwr8_cbcenc.c
    third-party/BearSSL/src/symcipher/aes_pwr8_ctr.c
    third-party/BearSSL/src/symcipher/aes_pwr8_ctrcbc.c
    third-party/BearSSL/src/symcipher/aes_small_cbcdec.c
    third-party/BearSSL/src/symcipher/aes_small_cbcenc.c
    third-party/BearSSL/src/symcipher/aes_small_ctr.c
    third-party/BearSSL/src/symcipher/aes_small_ctrcbc.c
    third-party/BearSSL/src/symcipher/aes_small_dec.c
    third-party/BearSSL/src/symcipher/aes_small_enc.c
    third-party/BearSSL/src/symcipher/aes_x86ni.c
    third-party/BearSSL/src/symcipher/aes_x86ni_cbcdec.c
    third-party/BearSSL/src/symcipher/aes_x86ni_cbcenc.c
    third-party/BearSSL/src/symcipher/aes_x86ni_ctr.c
    third-party/BearSSL/src/symcipher/aes_x86ni_ctrcbc.c
    third-party/BearSSL/src/symcipher/chacha20_ct.c
    third-party/BearSSL/src/symcipher/chacha20_sse2.c
    third-party/BearSSL/src/symcipher/des_ct.c
    third-party/BearSSL/src/symcipher/des_ct_cbcdec.c
    third-party/BearSSL/src/symcipher/des_ct_cbcenc.c
    third-party/BearSSL/src/symcipher/des_support.c
    third-party/BearSSL/src/symcipher/des_tab.c
    third-party/BearSSL/src/symcipher/des_tab_cbcdec.c
    third-party/BearSSL/src/symcipher/des_tab_cbcenc.c
    third-party/BearSSL/src/symcipher/poly1305_ctmul.c
    third-party/BearSSL/src/symcipher/poly1305_ctmul32.c
    third-party/BearSSL/src/symcipher/poly1305_ctmulq.c
    third-party/BearSSL/src/symcipher/poly1305_i15.c
    third-party/BearSSL/src/x509/asn1enc.c
    third-party/BearSSL/src/x509/encode_ec_pk8der.c
    third-party/BearSSL/src/x509/encode_ec_rawder.c
    third-party/BearSSL/src/x509/encode_rsa_pk8der.c
    third-party/BearSSL/src/x509/encode_rsa_rawder.c
    third-party/BearSSL/src/x509/skey_decoder.c
    third-party/BearSSL/src/x509/x509_decoder.c
    third-party/BearSSL/src/x509/x509_knownkey.c
    third-party/BearSSL/src/x509/x509_minimal.c
    third-party/BearSSL/src/x509/x509_minimal_full.c
    #third-party/BearSSL/test/test_crypto.c
    #third-party/BearSSL/test/test_math.c
    #third-party/BearSSL/test/test_speed.c
    #third-party/BearSSL/test/test_x509.c
    third-party/BearSSL/tools/brssl.c
    third-party/BearSSL/tools/certs.c
    third-party/BearSSL/tools/chain.c
    third-party/BearSSL/tools/client.c
    third-party/BearSSL/tools/errors.c
    third-party/BearSSL/tools/files.c
    third-party/BearSSL/tools/impl.c
    third-party/BearSSL/tools/keys.c
    third-party/BearSSL/tools/names.c
    third-party/BearSSL/tools/server.c
    third-party/BearSSL/tools/skey.c
    third-party/BearSSL/tools/sslio.c
    third-party/BearSSL/tools/ta.c
    third-party/BearSSL/tools/twrch.c
    third-party/BearSSL/tools/vector.c
    third-party/BearSSL/tools/verify.c
    third-party/BearSSL/tools/xmem.c)

set_target_properties(bearssl PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
)

target_include_directories(bearssl PUBLIC third-party/BearSSL/src)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "Build static libraries" FORCE)
set(CURL_STATICLIB ON CACHE BOOL "Build libcurl with static linking" FORCE)
set(CURL_DISABLE_LDAP OFF CACHE BOOL "Disable LDAP" FORCE)
set(CURL_STATIC_CRT ON CACHE BOOL "Link the static CRT" FORCE)
set(BUILD_CURL_EXE OFF CACHE BOOL "Build curl executable" FORCE)

# TODO: figure out what we actually need and dont
#set(CURL_DISABLE_DICT ON CACHE BOOL "Disable DICT" FORCE)
#set(CURL_DISABLE_FILE ON CACHE BOOL "Disable FILE" FORCE)
#set(CURL_DISABLE_FTP ON CACHE BOOL "Disable FTP" FORCE)
#set(CURL_DISABLE_GOPHER ON CACHE BOOL "Disable GOPHER" FORCE)
#set(CURL_DISABLE_MQTT ON CACHE BOOL "Disable MQTT" FORCE)
#set(CURL_DISABLE_POP3 ON CACHE BOOL "Disable POP3" FORCE)
#set(CURL_DISABLE_RTSP ON CACHE BOOL "Disable RTSP" FORCE)
#set(CURL_DISABLE_SMB ON CACHE BOOL "Disable SMB" FORCE)
#set(CURL_DISABLE_SMTP ON CACHE BOOL "Disable SMTP" FORCE)
#set(CURL_DISABLE_TELNET ON CACHE BOOL "Disable TELNET" FORCE)
#set(CURL_DISABLE_TFTP ON CACHE BOOL "Disable TFTP" FORCE)

set(_ssl_enabled ON)
set(USE_BEARSSL ON)
include_directories(SYSTEM ./third-party/BearSSL/inc)
set(_valid_default_ssl_backend TRUE)
set(_curl_ca_bundle_supported TRUE)
add_subdirectory(third-party/curl)
set_target_properties(libcurl_static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
)

#cblosc2
set(BLOSC_IS_SUBPROJECT TRUE)
set(BLOSC_INSTALL FALSE)
set(BUILD_STATIC ON)
set(BUILD_SHARED OFF)
set(BUILD_TESTS OFF)
set(BUILD_BENCHMARKS OFF)
set(BUILD_EXAMPLES OFF)
set(BUILD_FUZZERS OFF)
add_subdirectory(third-party/c-blosc2)
set_target_properties(blosc2_static PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
)

add_library(volcano_static STATIC volcano.c)
target_compile_definitions(volcano_static PUBLIC MINILIBS_STATIC_LIB)
target_link_libraries(volcano_static PRIVATE
    libcurl_static
    bearssl
    blosc2_static
)
if(WIN32)
  target_link_libraries(volcano_static PRIVATE ws2_32 crypt32 normaliz wldap32)
endif()

# 3. Dynamic library
add_library(volcano_shared SHARED volcano.c)
target_compile_definitions(volcano_shared PUBLIC MINILIBS_DYNAMIC_LIB)
target_link_libraries(volcano_shared PRIVATE
    libcurl_static
    bearssl
    blosc2_static
)
if(WIN32)
  target_link_libraries(volcano_shared PRIVATE ws2_32 crypt32 normaliz wldap32)
  target_compile_definitions(volcano_shared PRIVATE CURL_STATICLIB)
endif()

# Set properties for the shared library
set_target_properties(volcano_shared PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)



target_link_libraries(volcanotest PUBLIC libcurl_static bearssl blosc2_static)
target_link_libraries(volcano PUBLIC libcurl_static bearssl blosc2_static)

target_include_directories(volcanotest PUBLIC third-party/curl/include third-party/c-blosc2/include)
target_include_directories(volcano PUBLIC third-party/curl/include third-party/c-blosc2/include)

if(WIN32)
  target_compile_definitions(volcanotest PUBLIC CURL_STATICLIB)
  target_compile_definitions(volcano PUBLIC CURL_STATICLIB)
  target_link_libraries(volcanotest PUBLIC ws2_32 crypt32 normaliz wldap32)
  target_link_libraries(volcano PUBLIC ws2_32 crypt32 normaliz wldap32)
endif()



# Installation rules
install(TARGETS volcano volcanotest volcano_static volcano_shared
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)
install(FILES volcano.h DESTINATION include)