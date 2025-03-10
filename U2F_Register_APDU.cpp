/*
U2FDevice - A program to allow Raspberry Pi Zeros to act as U2F tokens
Copyright (C) 2018  Michael Kuc

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "U2F_Register_APDU.hpp"
#include "APDU.hpp"
#include "Certificates.hpp"
#include "Signature.hpp"
#include "U2FMessage.hpp"
#include "micro-ecc/uECC.h"
#include "u2f.hpp"
#include <cstring>
#include <exception>
#include <iostream>
#include <mbedtls/sha256.h>

using namespace std;

U2F_Register_APDU::U2F_Register_APDU(const U2F_Msg_CMD& msg, const vector<uint8_t>& data)
    : U2F_Msg_CMD{ msg } {
	if (data.size() != 64) {
		// Incorrect registration size
		throw APDU_STATUS::SW_WRONG_LENGTH;
	} else if ((p1 != 0x00 && p1 != 0x03) || p2 != 0x00) // According to spec, 0x03 not allowed here
	                                                     // However, browsers seem to do it, so...
	{
		// Invalid U2F Message (APDU) parameters detected
		throw APDU_STATUS::SW_INS_NOT_SUPPORTED;
	}

	copy(data.data() + 0, data.data() + 32, challengeP.begin());
	copy(data.data() + 32, data.data() + 64, appP.begin());

	// Use crypto lib to generate keypair
	Storage::PrivKey privKey{};
	Storage::PubKey pubKey{};

	// First byte must be 0x04 for some reason
	pubKey[0] = 0x04;

	uECC_make_key(pubKey.data() + 1, privKey.data(), uECC_secp256r1());

	this->keyH = Storage::appParams.size(); // To increment the key handle
	Storage::appParams[this->keyH] = appP;
	Storage::privKeys[this->keyH] = privKey;
	Storage::pubKeys[this->keyH] = pubKey;
	Storage::keyCounts[this->keyH] = 0;
}

void U2F_Register_APDU::respond(const uint32_t channelID) const {
	U2FMessage m{};
	m.cid = channelID;
	m.cmd = U2FHID_MSG;

	auto& response = m.data;
	const auto appParam = Storage::appParams[this->keyH];
	const auto pubKey = Storage::pubKeys[this->keyH];

	response.push_back(0x05);
	copy(pubKey.begin(), pubKey.end(), back_inserter(response));
	response.push_back(sizeof(this->keyH));

	auto fakeKeyHBytes = reinterpret_cast<const uint8_t*>(&this->keyH);
	copy(fakeKeyHBytes, fakeKeyHBytes + sizeof(this->keyH), back_inserter(response));

	copy(attestCert, end(attestCert), back_inserter(response));

	// Gen signature
	Digest digest;
	{
		mbedtls_sha256_context shaContext;

		mbedtls_sha256_init(&shaContext);
		mbedtls_sha256_starts(&shaContext, 0);

		uint8_t byteReserved = 0;
		mbedtls_sha256_update(&shaContext, reinterpret_cast<unsigned char*>(&byteReserved), 1);

		mbedtls_sha256_update(&shaContext, reinterpret_cast<const unsigned char*>(appParam.data()),
		                      appParam.size());

		mbedtls_sha256_update(&shaContext,
		                      reinterpret_cast<const unsigned char*>(challengeP.data()),
		                      challengeP.size());

		mbedtls_sha256_update(&shaContext, reinterpret_cast<const unsigned char*>(&keyH),
		                      sizeof(keyH));

		mbedtls_sha256_update(&shaContext, reinterpret_cast<const unsigned char*>(pubKey.data()),
		                      pubKey.size());

		mbedtls_sha256_finish(&shaContext, digest.data());
		mbedtls_sha256_free(&shaContext);
	}

	Signature signature;
	uECC_sign(attestPrivKey, digest.data(), digest.size(), signature.data(), uECC_secp256r1());

	// Append signature as DER
	appendSignatureAsDER(response, signature);

	response.push_back(static_cast<uint16_t>(APDU_STATUS::SW_NO_ERROR) >> 8);
	response.push_back(static_cast<uint16_t>(APDU_STATUS::SW_NO_ERROR) & 0xff);

	m.write();
}
