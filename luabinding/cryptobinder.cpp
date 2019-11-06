#include <stdlib.h>
#include <string.h>
#include "platformutil.h"
#include "lua.hpp"

static int crypto_md5(lua_State *L) //String=md5(String)
{
    size_t avail_in;
    const unsigned char *next_in = (const unsigned char *) luaL_checklstring(L, 1, &avail_in);
    unsigned char md5res[16];
    md5_frombuffer(next_in,avail_in,md5res);
    lua_pushlstring(L, (const char *) md5res,16);
    return 1;
}

static int crypto_aes_encrypt(lua_State *L) //String=encrypt(String,String,String,bool)
{
    size_t avail_in,klen;
    const unsigned char *in = (const unsigned char *) luaL_checklstring(L, 1, &avail_in);
    const unsigned char *key = (const unsigned char *) luaL_checklstring(L, 2, &klen);
    const unsigned char *iv = (const unsigned char *) luaL_optstring(L, 3,NULL);
    bool mode=lua_toboolean(L,4);

    if (!((klen==16)||(klen==24)||(klen==32))) {
		lua_pushfstring(L,"Invalid Key length for AES:%d",klen);
		lua_error(L);
    }

    size_t out_size=(avail_in+15)&(~15);
    if (mode&&iv) { //PKCS7 padding
    	if ((avail_in&15)==0) //room for extra 16 bytes
        	out_size+=16;
    }
    unsigned char *out=(unsigned char *) malloc(out_size);
	size_t rem=out_size-avail_in;
	memcpy(out,in,avail_in);
	memset(out+avail_in,(mode&&iv)?rem:0,rem);
    aes_encrypt(out,out_size,key,klen,iv);
    lua_pushlstring(L, (const char *)out,out_size);
    free(out);
    return 1;
}

static int crypto_aes_decrypt(lua_State *L) //String=encrypt(String,String,String,bool)
{
    size_t avail_in,klen;
    const unsigned char *in = (const unsigned char *) luaL_checklstring(L, 1, &avail_in);
    const unsigned char *key = (const unsigned char *) luaL_checklstring(L, 2, &klen);
    const unsigned char *iv = (const unsigned char *) luaL_optstring(L, 3,NULL);
    bool mode=lua_toboolean(L,4);

    if (!((klen==16)||(klen==24)||(klen==32))) {
		lua_pushfstring(L,"Invalid Key length for AES:%d",klen);
		lua_error(L);
    }

    avail_in&=(~15);
    unsigned char *out=(unsigned char *) malloc(avail_in);
	memcpy(out,in,avail_in);
    aes_decrypt(out,avail_in,key,klen,iv);

    if (mode&&iv&&avail_in) //PKCS7: remove padding
    {
       unsigned char paddingval=out[avail_in-1];
       if ((paddingval==0)||(paddingval>0x10)) //Wrong padding byte
       {
   	    free(out);
		lua_pushstring(L,"Invalid PKCS#7 padding value");
		lua_error(L);
       }
       for (int p=avail_in-paddingval;p<avail_in;p++)
    	   if (out[p]!=paddingval)
    	   {
    	   	    free(out);
    			lua_pushstring(L,"Invalid PKCS#7 padding");
    			lua_error(L);
    	   }
       avail_in-=paddingval;
     }

    lua_pushlstring(L, (const char *)out,avail_in);
    free(out);
    return 1;
}


/*
** =========================================================================
** Register functions
** =========================================================================
*/

#if (LUA_VERSION_NUM >= 502)

#define luaL_register(L,n,f)	luaL_setfuncs(L,f,0)

#endif

void register_crypto(lua_State *L)
{
    const luaL_Reg crypto[] =
    {
            {"md5",      crypto_md5    },
	        {"aesEncrypt",      crypto_aes_encrypt    },
	        {"aesDecrypt",      crypto_aes_decrypt    },

        {NULL, NULL}
    };

    lua_newtable(L);
    luaL_register(L, NULL, crypto);
    lua_setglobal(L, "Cryptography");
}
