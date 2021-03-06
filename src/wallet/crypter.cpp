// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "crypter.h"
#include "script/script.h"
#include "script/standard.h"
#include "util.h"
#include <openssl/md5.h>
#include <string>
#include <vector>
#include <boost/foreach.hpp>
#include <openssl/aes.h>
#include <openssl/evp.h>
#include <iostream>
#include <fstream>

#include <openssl/pem.h> // For RSA Key Export

unsigned char chKeyBiblePay[256];
unsigned char chIVBiblePay[256];
bool fKeySetBiblePay;
// RSA
const unsigned int RSA_KEYLEN = 2048;
const unsigned int KEY_SERVER_PRI = 0;
const unsigned int KEY_SERVER_PUB = 1;
const unsigned int KEY_CLIENT_PUB = 2;
const unsigned int KEY_AES        = 3;
const unsigned int KEY_AES_IV     = 4;
const unsigned int KEY_CLIENT_PRI = 5;
// END RSA


bool CCrypter::SetKeyFromPassphrase(const SecureString& strKeyData, const std::vector<unsigned char>& chSalt, const unsigned int nRounds, const unsigned int nDerivationMethod)
{
    if (nRounds < 1 || chSalt.size() != WALLET_CRYPTO_SALT_SIZE)
        return false;

    int i = 0;
    if (nDerivationMethod == 0)
        i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(), &chSalt[0],
                          (unsigned char *)&strKeyData[0], strKeyData.size(), nRounds, chKey, chIV);

    if (i != (int)WALLET_CRYPTO_KEY_SIZE)
    {
        memory_cleanse(chKey, sizeof(chKey));
        memory_cleanse(chIV, sizeof(chIV));
        return false;
    }

    fKeySet = true;
    return true;
}

bool CCrypter::SetKey(const CKeyingMaterial& chNewKey, const std::vector<unsigned char>& chNewIV)
{
    if (chNewKey.size() != WALLET_CRYPTO_KEY_SIZE || chNewIV.size() != WALLET_CRYPTO_KEY_SIZE)
        return false;

    memcpy(&chKey[0], &chNewKey[0], sizeof chKey);
    memcpy(&chIV[0], &chNewIV[0], sizeof chIV);

    fKeySet = true;
    return true;
}

bool CCrypter::Encrypt(const CKeyingMaterial& vchPlaintext, std::vector<unsigned char> &vchCiphertext)
{
    if (!fKeySet)
        return false;

    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCK_SIZE - 1 bytes
    int nLen = vchPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
    vchCiphertext = std::vector<unsigned char> (nCLen);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (!ctx) return false;

    bool fOk = true;

    EVP_CIPHER_CTX_init(ctx);
    if (fOk) fOk = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKey, chIV) != 0;
    if (fOk) fOk = EVP_EncryptUpdate(ctx, &vchCiphertext[0], &nCLen, &vchPlaintext[0], nLen) != 0;
    if (fOk) fOk = EVP_EncryptFinal_ex(ctx, (&vchCiphertext[0]) + nCLen, &nFLen) != 0;
    EVP_CIPHER_CTX_cleanup(ctx);

    EVP_CIPHER_CTX_free(ctx);

    if (!fOk) return false;

    vchCiphertext.resize(nCLen + nFLen);
    return true;
}

bool CCrypter::Decrypt(const std::vector<unsigned char>& vchCiphertext, CKeyingMaterial& vchPlaintext)
{
    if (!fKeySet)
        return false;

    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = vchCiphertext.size();
    int nPLen = nLen, nFLen = 0;

    vchPlaintext = CKeyingMaterial(nPLen);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (!ctx) return false;

    bool fOk = true;

    EVP_CIPHER_CTX_init(ctx);
    if (fOk) fOk = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKey, chIV) != 0;
    if (fOk) fOk = EVP_DecryptUpdate(ctx, &vchPlaintext[0], &nPLen, &vchCiphertext[0], nLen) != 0;
    if (fOk) fOk = EVP_DecryptFinal_ex(ctx, (&vchPlaintext[0]) + nPLen, &nFLen) != 0;
    EVP_CIPHER_CTX_cleanup(ctx);

    EVP_CIPHER_CTX_free(ctx);

    if (!fOk) return false;

    vchPlaintext.resize(nPLen + nFLen);
    return true;
}

bool LoadBibleKey(std::string biblekey, std::string salt)
{
	const char* chBibleKey = biblekey.c_str();
	const char* chSalt = salt.c_str();
	OPENSSL_cleanse(chKeyBiblePay, sizeof(chKeyBiblePay));
    OPENSSL_cleanse(chIVBiblePay, sizeof(chIVBiblePay));
    EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha512(),(unsigned char *)chSalt,
		(unsigned char *)chBibleKey, 
		strlen(chBibleKey), 1,
		chKeyBiblePay, chIVBiblePay);
    fKeySetBiblePay = true;
    return true;
}

std::vector<unsigned char> StringToVector(std::string sData)
{
        std::vector<unsigned char> v(sData.begin(), sData.end());
		return v;
}

std::string VectorToString(std::vector<unsigned char> v)
{
        std::string s(v.begin(), v.end());
        return s;
}

std::string PubKeyToAddress(const CScript& scriptPubKey)
{
	CTxDestination address1;
    ExtractDestination(scriptPubKey, address1);
    CBitcoinAddress address2(address1);
    return address2.ToString();
}    

std::string RetrieveMd5(std::string s1)
{
	try
	{
		const char* chIn = s1.c_str();
		unsigned char digest2[16];
		MD5((unsigned char*)chIn, strlen(chIn), (unsigned char*)&digest2);
		char mdString2[33];
		for(int i = 0; i < 16; i++) sprintf(&mdString2[i*2], "%02x", (unsigned int)digest2[i]);
 		std::string xmd5(mdString2);
		return xmd5;
	}
    catch (std::exception &e)
	{
		return "";
	}
}

void PrintStratisKeyDebugInfo()
{
	std::string sKey1(reinterpret_cast<char*>(chKeyBiblePay));
	std::string sIV1(reinterpret_cast<char*>(chIVBiblePay));
	std::string sRow = "chKeyBiblePay: ";
	for (int i = 0; i < (int)sKey1.length(); i++)
	{
		int ichar = (int)sKey1[i];
		sRow += RoundToString(ichar,0) + ",";
	}
	sRow += "  chIVBiblePay: ";
	for (int i = 0; i < (int)sIV1.length(); i++)
	{
		int ichar = (int)sIV1[i];
		sRow += RoundToString(ichar,0) + ",";
	}
	LogPrintf(" \n StratisKeyDebugInfo: %s \n",sRow.c_str());
}

bool BibleEncrypt(std::vector<unsigned char> vchPlaintext, std::vector<unsigned char> &vchCiphertext)
{
	if (!fKeySetBiblePay) LoadBibleKey("biblepay","eb5a781ea9da2ef36");
    int nLen = vchPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE, nFLen = 0;
    vchCiphertext = std::vector<unsigned char> (nCLen);
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    bool fOk = true;
    EVP_CIPHER_CTX_init(ctx);
	if (fOk) fOk = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKeyBiblePay, chIVBiblePay);
    if (fOk) fOk = EVP_EncryptUpdate(ctx, &vchCiphertext[0], &nCLen, &vchPlaintext[0], nLen);
    if (fOk) fOk = EVP_EncryptFinal_ex(ctx, (&vchCiphertext[0])+nCLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);
    if (!fOk) return false;
    vchCiphertext.resize(nCLen + nFLen);
    return true;
}

bool BibleDecrypt(const std::vector<unsigned char>& vchCiphertext,std::vector<unsigned char>& vchPlaintext)
{
	LoadBibleKey("biblepay","eb5a781ea9da2ef36");
	int nLen = vchCiphertext.size();
    int nPLen = nLen, nFLen = 0;
    //EVP_CIPHER_CTX ctx;
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
    bool fOk = true;
    EVP_CIPHER_CTX_init(ctx);
    if (fOk) fOk = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, chKeyBiblePay, chIVBiblePay);
    if (fOk) fOk = EVP_DecryptUpdate(ctx, &vchPlaintext[0], &nPLen, &vchCiphertext[0], nLen);
    if (fOk) fOk = EVP_DecryptFinal_ex(ctx, (&vchPlaintext[0])+nPLen, &nFLen);
    EVP_CIPHER_CTX_free(ctx);
    if (!fOk) return false;
    vchPlaintext.resize(nPLen + nFLen);
    return true;
}


static bool EncryptSecret(const CKeyingMaterial& vMasterKey, const CKeyingMaterial &vchPlaintext, const uint256& nIV, std::vector<unsigned char> &vchCiphertext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_KEY_SIZE);
    if(!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Encrypt(*((const CKeyingMaterial*)&vchPlaintext), vchCiphertext);
}


// General secure AES 256 CBC encryption routine
bool EncryptAES256(const SecureString& sKey, const SecureString& sPlaintext, const std::string& sIV, std::string& sCiphertext)
{
    // max ciphertext len for a n bytes of plaintext is
    // n + AES_BLOCK_SIZE - 1 bytes
    int nLen = sPlaintext.size();
    int nCLen = nLen + AES_BLOCK_SIZE;
    int nFLen = 0;

    // Verify key sizes
    if(sKey.size() != 32 || sIV.size() != AES_BLOCK_SIZE) {
        LogPrintf("crypter EncryptAES256 - Invalid key or block size: Key: %d sIV:%d\n", sKey.size(), sIV.size());
        return false;
    }

    // Prepare output buffer
    sCiphertext.resize(nCLen);

    // Perform the encryption
    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (!ctx) return false;

    bool fOk = true;

    EVP_CIPHER_CTX_init(ctx);
    if (fOk) fOk = EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*) &sKey[0], (const unsigned char*) &sIV[0]);
    if (fOk) fOk = EVP_EncryptUpdate(ctx, (unsigned char*) &sCiphertext[0], &nCLen, (const unsigned char*) &sPlaintext[0], nLen);
    if (fOk) fOk = EVP_EncryptFinal_ex(ctx, (unsigned char*) (&sCiphertext[0])+nCLen, &nFLen);
    EVP_CIPHER_CTX_cleanup(ctx);

    EVP_CIPHER_CTX_free(ctx);

    if (!fOk) return false;

    sCiphertext.resize(nCLen + nFLen);
    return true;
}


static bool DecryptSecret(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCiphertext, const uint256& nIV, CKeyingMaterial& vchPlaintext)
{
    CCrypter cKeyCrypter;
    std::vector<unsigned char> chIV(WALLET_CRYPTO_KEY_SIZE);
    memcpy(&chIV[0], &nIV, WALLET_CRYPTO_KEY_SIZE);
    if(!cKeyCrypter.SetKey(vMasterKey, chIV))
        return false;
    return cKeyCrypter.Decrypt(vchCiphertext, *((CKeyingMaterial*)&vchPlaintext));
}

bool DecryptAES256(const SecureString& sKey, const std::string& sCiphertext, const std::string& sIV, SecureString& sPlaintext)
{
    // plaintext will always be equal to or lesser than length of ciphertext
    int nLen = sCiphertext.size();
    int nPLen = nLen, nFLen = 0;

    // Verify key sizes
    if(sKey.size() != 32 || sIV.size() != AES_BLOCK_SIZE) {
        LogPrintf("crypter DecryptAES256 - Invalid key or block size\n");
        return false;
    }

    sPlaintext.resize(nPLen);

    EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();

    if (!ctx) return false;

    bool fOk = true;

    EVP_CIPHER_CTX_init(ctx);
    if (fOk) fOk = EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, (const unsigned char*) &sKey[0], (const unsigned char*) &sIV[0]);
    if (fOk) fOk = EVP_DecryptUpdate(ctx, (unsigned char *) &sPlaintext[0], &nPLen, (const unsigned char *) &sCiphertext[0], nLen);
    if (fOk) fOk = EVP_DecryptFinal_ex(ctx, (unsigned char *) (&sPlaintext[0])+nPLen, &nFLen);
    EVP_CIPHER_CTX_cleanup(ctx);

    EVP_CIPHER_CTX_free(ctx);

    if (!fOk) return false;

    sPlaintext.resize(nPLen + nFLen);
    return true;
}


static bool DecryptKey(const CKeyingMaterial& vMasterKey, const std::vector<unsigned char>& vchCryptedSecret, const CPubKey& vchPubKey, CKey& key)
{
    CKeyingMaterial vchSecret;
    if(!DecryptSecret(vMasterKey, vchCryptedSecret, vchPubKey.GetHash(), vchSecret))
        return false;

    if (vchSecret.size() != 32)
        return false;

    key.Set(vchSecret.begin(), vchSecret.end(), vchPubKey.IsCompressed());
    return key.VerifyPubKey(vchPubKey);
}

/* R ANDREWS - BIBLEPAY - 9/13/2018 - ADD SUPPORT FOR RSA */


int RSA_WRITE_KEY_TO_FILE(FILE *file, int key, EVP_PKEY *rKey)
{
	switch(key) 
	{
		case KEY_SERVER_PRI:
			 if(!PEM_write_PrivateKey(file, rKey, NULL, NULL, 0, 0, NULL))         return -1;
			 break;
		case KEY_CLIENT_PRI:
			 if(!PEM_write_PrivateKey(file, rKey, NULL, NULL, 0, 0, NULL))         return -1;
			 break;
		case KEY_SERVER_PUB:
			 if(!PEM_write_PUBKEY(file, rKey))                                     return -1;
			 break;
		case KEY_CLIENT_PUB:
			 if(!PEM_write_PUBKEY(file, rKey))                                     return -1;
			 break;
		default:
			return -1;
	}
	return 1;
}


std::vector<char> ReadAllBytes(char const* filename)
{
    std::ifstream ifs(filename, std::ios::binary|std::ios::ate);
    std::ifstream::pos_type pos = ifs.tellg();
    std::vector<char>  result(pos);
    ifs.seekg(0, std::ios::beg);
    ifs.read(&result[0], pos);
    return result;
}

int RSA_GENERATE_KEYPAIR(std::string sPublicKeyPath, std::string sPrivateKeyPath)
{
	if (sPublicKeyPath.empty() || sPrivateKeyPath.empty()) return -1;
    EVP_PKEY *remotePublicKey = NULL;
	EVP_PKEY_CTX *context = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
	if(EVP_PKEY_keygen_init(context) <= 0) return -1;
	if(EVP_PKEY_CTX_set_rsa_keygen_bits(context, RSA_KEYLEN) <= 0) return -1;
	if(EVP_PKEY_keygen(context, &remotePublicKey) <= 0) return -1;
	EVP_PKEY_CTX_free(context);
    FILE *outFile = fopen(sPublicKeyPath.c_str(), "w");
	RSA_WRITE_KEY_TO_FILE(outFile, KEY_CLIENT_PUB, remotePublicKey);
	fclose(outFile);
	FILE *outFile2 = fopen(sPrivateKeyPath.c_str(), "w");
	RSA_WRITE_KEY_TO_FILE(outFile2, KEY_CLIENT_PRI, remotePublicKey);
	fclose(outFile2);
	return 1;
}

EVP_PKEY *RSA_LOAD_PUBKEY(const char *file)
{
	RSA *rsa_pkey = NULL;
	BIO *rsa_pkey_file = NULL;
	EVP_PKEY *pkey = EVP_PKEY_new();
	// Create a new BIO file structure to be used with PEM file
	rsa_pkey_file = BIO_new(BIO_s_file());
	if (rsa_pkey_file == NULL)
	{
		fprintf(stderr, "RSA_LOAD_PUBKEY::Error creating a new BIO file.\n");
		goto end;
	}
	
	// Read PEM file using BIO's file structure
	if (BIO_read_filename(rsa_pkey_file, file) <= 0)
	{
		fprintf(stderr, "RSA_LOAD_PUBKEY::Error opening %s\n",file);
		goto end;
	}

	// Read RSA based PEM file into rsa_pkey structure
	if (!PEM_read_bio_RSA_PUBKEY(rsa_pkey_file, &rsa_pkey, NULL, NULL))
	{
		fprintf(stderr, "Error loading RSA Public Key File.\n");
		goto end;
	}

	// Populate pkey with the rsa key. rsa_pkey is owned by pkey, therefore if we free pkey, rsa_pkey will be freed  too
    if (!EVP_PKEY_assign_RSA(pkey, rsa_pkey))
    {
        fprintf(stderr, "Error assigning EVP_PKEY_assign_RSA: failed.\n");
        goto end;
    }

end:
	if (rsa_pkey_file != NULL)
		BIO_free(rsa_pkey_file);
	if (pkey == NULL)
	{
		fprintf(stderr, "RSA_LOAD_PUBKEY::Error unable to load %s\n", file);
	}
	return(pkey);
}

EVP_PKEY *RSA_LOAD_PRIVKEY(const char *file)
{
	RSA *rsa_pkey = NULL;
	BIO *rsa_pkey_file = NULL;
	EVP_PKEY *pkey = EVP_PKEY_new();
	// Create a new BIO file structure to be used with PEM file
	rsa_pkey_file = BIO_new(BIO_s_file());
	if (rsa_pkey_file == NULL)
	{
		fprintf(stderr, "Error creating a new BIO file.\n");
		goto end;
	}
	// Read PEM file using BIO's file structure
	if (BIO_read_filename(rsa_pkey_file, file) <= 0)
	{
		fprintf(stderr, "Error opening %s\n",file);
		goto end;
	}
	// Read RSA based PEM file into rsa_pkey structure
	if (!PEM_read_bio_RSAPrivateKey(rsa_pkey_file, &rsa_pkey, NULL, NULL))
	{
		fprintf(stderr, "Error loading RSA Private Key File.\n");
		goto end;
	}
	// Populate pkey with the rsa key. rsa_pkey is owned by pkey,
	// therefore if we free pkey, rsa_pkey will be freed  too
    if (!EVP_PKEY_assign_RSA(pkey, rsa_pkey))
    {
        fprintf(stderr, "Error assigning EVP_PKEY_assign_RSA: failed.\n");
        goto end;
    }

end:
	if (rsa_pkey_file != NULL)
		BIO_free(rsa_pkey_file);
	if (pkey == NULL)
	{
		fprintf(stderr, "RSA_Load_Private_Key::Error unable to load %s\n", file);
	}
	return(pkey);
}

unsigned char *RSA_ENCRYPT_CHAR(std::string sPubKeyPath, unsigned char *plaintext, int plaintext_length, int& cipher_len, std::string& sError)
{
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	EVP_CIPHER_CTX_init(ctx);
	EVP_PKEY *pkey;
	unsigned char iv[EVP_MAX_IV_LENGTH];
	unsigned char *encrypted_key;
	int encrypted_key_length;
	uint32_t eklen_n;
	unsigned char *ciphertext;
	// Load RSA Public Key File (usually this is a PEM ASCII file with boundaries or armor)
	pkey = RSA_LOAD_PUBKEY(sPubKeyPath.c_str());
	if (pkey == NULL)
	{
	    ciphertext = (unsigned char*)malloc(1);
		sError = "Error loading public key.";
		return ciphertext;
	}
	encrypted_key = (unsigned char*)malloc(EVP_PKEY_size(pkey));
	encrypted_key_length = EVP_PKEY_size(pkey);
	if (!EVP_SealInit(ctx, EVP_des_ede_cbc(), &encrypted_key, &encrypted_key_length, iv, &pkey, 1))
	{
		fprintf(stdout, "EVP_SealInit: failed.\n");
	}
	eklen_n = htonl(encrypted_key_length);
	int size_header = sizeof(eklen_n) + encrypted_key_length + EVP_CIPHER_iv_length(EVP_des_ede_cbc());
	/* compute max ciphertext len, see man EVP_CIPHER */
	int max_cipher_len = plaintext_length + EVP_CIPHER_CTX_block_size(ctx) - 1;
	ciphertext = (unsigned char*)malloc(size_header + max_cipher_len);
	/* Write out the encrypted key length, then the encrypted key, then the iv (the IV length is fixed by the cipher we have chosen). */
	int pos = 0;
	memcpy(ciphertext + pos, &eklen_n, sizeof(eklen_n));
	pos += sizeof(eklen_n);
	memcpy(ciphertext + pos, encrypted_key, encrypted_key_length);
	pos += encrypted_key_length;
	memcpy(ciphertext + pos, iv, EVP_CIPHER_iv_length(EVP_des_ede_cbc()));
	pos += EVP_CIPHER_iv_length(EVP_des_ede_cbc());
	/* Process the plaintext data and write the encrypted data to the ciphertext. cipher_len is filled with the length of ciphertext generated, len is the size of plaintext in bytes
	 * Also we have our updated position, we can skip the header via ciphertext + pos */
	int total_len = 0;
	int bytes_processed = 0;
	if (!EVP_SealUpdate(ctx, ciphertext + pos, &bytes_processed, plaintext, plaintext_length))
	{
		fprintf(stdout, "EVP_SealUpdate: failed.\n");
	}

	LogPrintf(" precipherlen %f ", bytes_processed);
	total_len += bytes_processed;
	pos += bytes_processed;
	if (!EVP_SealFinal(ctx, ciphertext + pos, &bytes_processed))
	{
		fprintf(stdout, "RSA_Encrypt::EVP_SealFinal: failed.\n");
	}
	total_len += bytes_processed;
	cipher_len = total_len;
	EVP_PKEY_free(pkey);
	free(encrypted_key);
	EVP_CIPHER_CTX_free(ctx);
	return ciphertext;
}

void RSA_Encrypt_File(std::string sPubKeyPath, std::string sSourcePath, std::string sEncryptPath, std::string& sError)
{
	std::vector<char> vMessage = ReadAllBytes(sSourcePath.c_str());
	std::vector<unsigned char> uData(vMessage.begin(), vMessage.end());
	unsigned char *ciphertext;
	int cipher_len = 0;
	unsigned char *long_ciphertext = (unsigned char *)malloc(uData.size() + 10000);
	memcpy(long_ciphertext, &uData[0], uData.size());
	size_t messageLength = uData.size() + 10000;
	ciphertext = RSA_ENCRYPT_CHAR(sPubKeyPath, long_ciphertext, messageLength, cipher_len, sError);
	if (sError.empty())
	{
		std::ofstream fd(sEncryptPath.c_str());
		fd.write((const char*)ciphertext, cipher_len);
		fd.close();
	}
}

unsigned char *RSA_DECRYPT_CHAR(std::string sPriKeyPath, unsigned char *ciphertext, int ciphrtext_size, int& plaintxt_len, std::string& sError)
{
	EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
	EVP_CIPHER_CTX_init(ctx);
	EVP_PKEY *pkey;
	unsigned char *encrypted_key;
	unsigned int encrypted_key_length;
	uint32_t eklen_n;
	unsigned char iv[EVP_MAX_IV_LENGTH];
	int bytes_processed = 0;
	// the length of udata is at most ciphertextlen + ciphers block size.
	int ciphertext_len = ciphrtext_size + EVP_CIPHER_block_size(EVP_des_ede_cbc());
	unsigned char *plaintext = (unsigned char *)malloc(ciphertext_len);
	pkey = RSA_LOAD_PRIVKEY(sPriKeyPath.c_str());
	if (pkey==NULL)
	{
		sError = "No private key provided.";
		return plaintext;
	}
	encrypted_key = (unsigned char*)malloc(EVP_PKEY_size(pkey));
	int pos = 0;
	memcpy(&eklen_n, ciphertext + pos, sizeof(eklen_n));
	pos += sizeof(eklen_n);
	encrypted_key_length = ntohl(eklen_n);
	memcpy(encrypted_key, ciphertext + pos, encrypted_key_length);
	pos += encrypted_key_length;
	memcpy(iv, ciphertext + pos, EVP_CIPHER_iv_length(EVP_des_ede_cbc()));
	pos += EVP_CIPHER_iv_length(EVP_des_ede_cbc());
	int total_len = 0;
	// Now we have our encrypted_key and the iv we can decrypt the reamining
	if (!EVP_OpenInit(ctx, EVP_des_ede_cbc(), encrypted_key, encrypted_key_length, iv, pkey))
	{
		fprintf(stderr, "RSADecrypt::EVP_OpenInit: failed.\n");
		sError = "EVP_OpenInit Failed.";
		return plaintext;
	}
	if (!EVP_OpenUpdate(ctx, plaintext, &bytes_processed, ciphertext + pos, ciphrtext_size))
	{
		fprintf(stderr, "RSA::Decrypt::EVP_OpenUpdate: failed.\n");
		sError = "EVP_OpenUpdate failed.";
		return plaintext;
	}
	total_len += bytes_processed;
	if (!EVP_OpenFinal(ctx, plaintext + total_len, &bytes_processed))
	{
		fprintf(stderr, "RSA::Decrypt::EVP_OpenFinal warning: failed.\n");
	}
	total_len += bytes_processed;

	EVP_PKEY_free(pkey);
	free(encrypted_key);

	EVP_CIPHER_CTX_free(ctx);
	plaintxt_len = total_len;
	return plaintext;
}

void RSA_Decrypt_File(std::string sPrivKeyPath, std::string sSourcePath, std::string sDecryptPath, std::string sError)
{
	std::vector<char> vMessage = ReadAllBytes(sSourcePath.c_str());
	std::vector<unsigned char> uData(vMessage.begin(), vMessage.end());
	size_t messageLength = uData.size();
	int plaintextsize = 0;
	unsigned char *decrypted;
	decrypted = RSA_DECRYPT_CHAR(sPrivKeyPath, &uData[0], messageLength, plaintextsize, sError);
	if (sError.empty())
	{
		if (plaintextsize < 10000)
		{
			sError = "RSA_Decrypt_File::Encrypted file is corrupted.";
			return;
		}
		unsigned char *short_plaintext = (unsigned char *)malloc(plaintextsize - 10000);
		memcpy(short_plaintext, decrypted, plaintextsize - 10000);
		std::ofstream fd(sDecryptPath.c_str());
		fd.write((const char*)short_plaintext, plaintextsize - 10000);
		fd.close();
	}
}


std::string RSA_Encrypt_String(std::string sPubKeyPath, std::string sData, std::string& sError)
{
	std::string sEncPath =  GetSANDirectory2() + "temp";
	std::ofstream fd(sEncPath.c_str());
	fd.write(sData.c_str(), sizeof(char)*sData.size());
	fd.close();
	std::string sTargPath = GetSANDirectory2() + "temp_enc";
	RSA_Encrypt_File(sPubKeyPath, sEncPath, sTargPath, sError);
	if (!sError.empty()) return "";
	std::vector<char> vOut = ReadAllBytes(sTargPath.c_str());
	std::string sResults(vOut.begin(), vOut.end());
	return sResults;
}

std::string RSA_Decrypt_String(std::string sPrivKeyPath, std::string sData, std::string& sError)
{
	std::string sEncPath =  GetSANDirectory2() + "temp_dec";
	std::ofstream fd(sEncPath.c_str());
	fd.write(sData.c_str(), sizeof(char)*sData.size());

	fd.close();
	std::string sTargPath = GetSANDirectory2() + "temp_dec_unenc";
	RSA_Decrypt_File(sPrivKeyPath, sEncPath, sTargPath, sError);
	if (!sError.empty()) return "";
	std::vector<char> vOut = ReadAllBytes(sTargPath.c_str());
	std::string sResults(vOut.begin(), vOut.end());
	return sResults;
}


/* END OF RSA SUPPORT - BIBLEPAY */





bool CCryptoKeyStore::SetCrypted()
{
    LOCK(cs_KeyStore);
    if (fUseCrypto)
        return true;
    if (!mapKeys.empty())
        return false;
    fUseCrypto = true;
    return true;
}

bool CCryptoKeyStore::Lock(bool fAllowMixing)
{
    if (!SetCrypted())
        return false;

    if(!fAllowMixing) {
        LOCK(cs_KeyStore);
        vMasterKey.clear();
    }

    fOnlyMixingAllowed = fAllowMixing;
    NotifyStatusChanged(this);
    return true;
}

bool CCryptoKeyStore::Unlock(const CKeyingMaterial& vMasterKeyIn, bool fForMixingOnly)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;

        bool keyPass = false;
        bool keyFail = false;
        CryptedKeyMap::const_iterator mi = mapCryptedKeys.begin();
        for (; mi != mapCryptedKeys.end(); ++mi)
        {
            const CPubKey &vchPubKey = (*mi).second.first;
            const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
            CKey key;
            if (!DecryptKey(vMasterKeyIn, vchCryptedSecret, vchPubKey, key))
            {
                keyFail = true;
                break;
            }
            keyPass = true;
            if (fDecryptionThoroughlyChecked)
                break;
        }
        if (keyPass && keyFail)
        {
            LogPrintf("The wallet is probably corrupted: Some keys decrypt but not all.\n");
            assert(false);
        }
        if (keyFail || !keyPass)
            return false;
        vMasterKey = vMasterKeyIn;
        fDecryptionThoroughlyChecked = true;
    }
    fOnlyMixingAllowed = fForMixingOnly;
    NotifyStatusChanged(this);
    return true;
}

bool CCryptoKeyStore::AddKeyPubKey(const CKey& key, const CPubKey &pubkey)
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::AddKeyPubKey(key, pubkey);

        if (IsLocked(true))
            return false;

        std::vector<unsigned char> vchCryptedSecret;
        CKeyingMaterial vchSecret(key.begin(), key.end());
        if (!EncryptSecret(vMasterKey, vchSecret, pubkey.GetHash(), vchCryptedSecret))
            return false;

        if (!AddCryptedKey(pubkey, vchCryptedSecret))
            return false;
    }
    return true;
}


bool CCryptoKeyStore::AddCryptedKey(const CPubKey &vchPubKey, const std::vector<unsigned char> &vchCryptedSecret)
{
    {
        LOCK(cs_KeyStore);
        if (!SetCrypted())
            return false;

        mapCryptedKeys[vchPubKey.GetID()] = make_pair(vchPubKey, vchCryptedSecret);
    }
    return true;
}

bool CCryptoKeyStore::GetKey(const CKeyID &address, CKey& keyOut) const
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::GetKey(address, keyOut);

        CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
        if (mi != mapCryptedKeys.end())
        {
            const CPubKey &vchPubKey = (*mi).second.first;
            const std::vector<unsigned char> &vchCryptedSecret = (*mi).second.second;
            return DecryptKey(vMasterKey, vchCryptedSecret, vchPubKey, keyOut);
        }
    }
    return false;
}

bool CCryptoKeyStore::GetPubKey(const CKeyID &address, CPubKey& vchPubKeyOut) const
{
    {
        LOCK(cs_KeyStore);
        if (!IsCrypted())
            return CBasicKeyStore::GetPubKey(address, vchPubKeyOut);

        CryptedKeyMap::const_iterator mi = mapCryptedKeys.find(address);
        if (mi != mapCryptedKeys.end())
        {
            vchPubKeyOut = (*mi).second.first;
            return true;
        }
        // Check for watch-only pubkeys
        return CBasicKeyStore::GetPubKey(address, vchPubKeyOut);
    }
    return false;
}

bool CCryptoKeyStore::EncryptKeys(CKeyingMaterial& vMasterKeyIn)
{
    {
        LOCK(cs_KeyStore);
        if (!mapCryptedKeys.empty() || IsCrypted())
            return false;

        fUseCrypto = true;
        BOOST_FOREACH(KeyMap::value_type& mKey, mapKeys)
        {
            const CKey &key = mKey.second;
            CPubKey vchPubKey = key.GetPubKey();
            CKeyingMaterial vchSecret(key.begin(), key.end());
            std::vector<unsigned char> vchCryptedSecret;
            if (!EncryptSecret(vMasterKeyIn, vchSecret, vchPubKey.GetHash(), vchCryptedSecret))
                return false;
            if (!AddCryptedKey(vchPubKey, vchCryptedSecret))
                return false;
        }
        mapKeys.clear();
    }
    return true;
}




