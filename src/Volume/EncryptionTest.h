/*
 Derived from source code of TrueCrypt 7.1a, which is
 Copyright (c) 2008-2012 TrueCrypt Developers Association and which is governed
 by the TrueCrypt License 3.0.

 Modifications and additions to the original source code (contained in this file)
 and all other portions of this file are Copyright (c) 2013-2025 AM Crypto
 and are governed by the Apache License 2.0 the full text of which is
 contained in the file License.txt included in VeraCrypt binary and source
 code distribution packages.
*/

#ifndef TC_HEADER_Encryption_EncryptionTest
#define TC_HEADER_Encryption_EncryptionTest

#include "Platform/Platform.h"
#include "Common/Crypto.h"

namespace VeraCrypt
{
	class EncryptionTest
	{
	public:
		static void TestAll ();
		static void TestAll (bool enableCpuEncryptionSupport);

	protected:
		static void TestCiphers ();
		static void TestLegacyModes ();
		static void TestPkcs5 ();
		static void TestXts ();
		static void TestXtsAES ();

	struct XtsTestVector
	{
		uint8 key1[32];
		uint8 key2[32];
		uint8 dataUnitNo[8];
		unsigned int blockNo;
		uint8 plaintext[ENCRYPTION_DATA_UNIT_SIZE];
		uint8 ciphertext[ENCRYPTION_DATA_UNIT_SIZE];
	};

	static const XtsTestVector XtsTestVectors[];

	private:
		EncryptionTest ();
		virtual ~EncryptionTest ();
		EncryptionTest (const EncryptionTest &);
		EncryptionTest &operator= (const EncryptionTest &);
	};
}

#endif // TC_HEADER_Encryption_EncryptionTest
