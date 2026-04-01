/*****************************************************************************
 * PokerTH - The open source texas holdem engine                             *
 * Copyright (C) 2006-2012 Felix Hammer, Florian Thauer, Lothar May          *
 *                                                                           *
 * This program is free software: you can redistribute it and/or modify      *
 * it under the terms of the GNU Affero General Public License as            *
 * published by the Free Software Foundation, either version 3 of the        *
 * License, or (at your option) any later version.                           *
 *                                                                           *
 * This program is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU Affero General Public License for more details.                       *
 *                                                                           *
 * You should have received a copy of the GNU Affero General Public License  *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.     *
 *                                                                           *
 *                                                                           *
 * Additional permission under GNU AGPL version 3 section 7                  *
 *                                                                           *
 * If you modify this program, or any covered work, by linking or            *
 * combining it with the OpenSSL project's OpenSSL library (or a             *
 * modified version of that library), containing parts covered by the        *
 * terms of the OpenSSL or SSLeay licenses, the authors of PokerTH           *
 * (Felix Hammer, Florian Thauer, Lothar May) grant you additional           *
 * permission to convey the resulting work.                                  *
 * Corresponding Source for a non-source form of such a combination          *
 * shall include the source code for the parts of OpenSSL used as well       *
 * as that of the covered work.                                              *
 *****************************************************************************/
/* Helper class for crypt functions. */

#ifndef _CRYPTHELPER_H_
#define _CRYPTHELPER_H_

#include <string>
#include <vector>
#include <array>
#include <climits>

constexpr int MD5_DATA_SIZE = 16;
constexpr int SHA1_DATA_SIZE = 20;

constexpr int AES_BLOCK_SIZE = 16;
constexpr unsigned ADD_PADDING(unsigned x) {
	return x <= UINT_MAX - 15 ? ((((x) + 15) >> 4) << 4) : 0;
}

class HashBuf
{
public:
	virtual ~HashBuf() noexcept;

	std::string ToString() const;
	bool FromString(const std::string &text);
	bool IsZero() const;

	bool operator==(const HashBuf &other) const;
	bool operator<(const HashBuf &other) const;

	virtual unsigned char *GetData() = 0;
	virtual const unsigned char *GetData() const = 0;
	virtual int GetDataSize() const = 0;
};

class MD5Buf : public HashBuf
{
public:
	MD5Buf();

	unsigned char *GetData() override;
	const unsigned char *GetData() const override;
	int GetDataSize() const override;

private:
	std::array<unsigned char, MD5_DATA_SIZE> m_data;
};

class SHA1Buf : public HashBuf
{
public:
	SHA1Buf();

	unsigned char *GetData() override;
	const unsigned char *GetData() const override;
	int GetDataSize() const override;

private:
	std::array<unsigned char, SHA1_DATA_SIZE> m_data;
};

class CryptHelper
{
public:

	static bool MD5Sum(const std::string &fileName, MD5Buf &buf);
	static bool SHA1Hash(const unsigned char *data, unsigned dataSize, SHA1Buf &buf);
	static bool HMACSha1(const unsigned char *keyData, unsigned keySize, const unsigned char *plainData, unsigned plainSize, SHA1Buf &buf);
	static bool AES128Encrypt(const unsigned char *keyData, unsigned keySize, const std::string &plainStr, std::vector<unsigned char> &outCipher);
	static bool AES128Decrypt(const unsigned char *keyData, unsigned keySize, const unsigned char *cipher, unsigned cipherSize, std::string &outPlain);
	static void SecureClearMemory(void *ptr, size_t len);

private:
	static void BytesToKey(const unsigned char *keyData, unsigned keySize, unsigned char *key, unsigned char *iv);
};

#endif
