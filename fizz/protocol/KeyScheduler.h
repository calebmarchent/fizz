/*
 *  Copyright (c) 2018-present, Facebook, Inc.
 *  All rights reserved.
 *
 *  This source code is licensed under the BSD-style license found in the
 *  LICENSE file in the root directory of this source tree.
 */

#pragma once

#include <fizz/crypto/KeyDerivation.h>
#include <fizz/crypto/aead/Aead.h>
#include <folly/Optional.h>

namespace fizz {

enum class EarlySecrets {
  ExternalPskBinder,
  ResumptionPskBinder,
  ClientEarlyTraffic,
  EarlyExporter
};

enum class HandshakeSecrets { ClientHandshakeTraffic, ServerHandshakeTraffic };

enum class MasterSecrets { ExporterMaster, ResumptionMaster };

enum class AppTrafficSecrets { ClientAppTraffic, ServerAppTraffic };

using SecretType = boost::
    variant<EarlySecrets, HandshakeSecrets, MasterSecrets, AppTrafficSecrets>;

struct DerivedSecret {
  std::vector<uint8_t> secret;
  SecretType type;

  DerivedSecret(std::vector<uint8_t> secretIn, SecretType typeIn)
      : secret(std::move(secretIn)), type(typeIn) {}

  DerivedSecret(folly::ByteRange secretIn, SecretType typeIn)
      : secret(secretIn.begin(), secretIn.end()), type(typeIn) {}
};

/**
 * Keeps track of the TLS 1.3 key derivation schedule.
 */
class KeyScheduler {
 public:
  explicit KeyScheduler(std::unique_ptr<KeyDerivation> deriver)
      : deriver_(std::move(deriver)) {}
  virtual ~KeyScheduler() = default;

  /**
   * Derives the early secret. Must be in uninitialized state.
   */
  virtual void deriveEarlySecret(folly::ByteRange psk);

  /**
   * Derives the master secert. Must be in early secret state.
   */
  virtual void deriveHandshakeSecret();

  /**
   * Derives the master secret with a DH secret. Must be in uninitialized or
   * early secret state.
   */
  virtual void deriveHandshakeSecret(folly::ByteRange ecdhe);

  /**
   * Derives the master secert. Must be in handshake secret state.
   */
  virtual void deriveMasterSecret();

  /**
   * Derives the app traffic secrets given the handshake context. Must be in
   * master secret state. Note that this does not clear the master secret.
   */
  virtual void deriveAppTrafficSecrets(folly::ByteRange transcript);

  /**
   * Clears the master secret. Must be in master secret state.
   */
  virtual void clearMasterSecret();

  /**
   * Performs a key update on the client traffic key. Traffic secrets must be
   * derived.
   */
  virtual uint32_t clientKeyUpdate();

  /**
   * Performs a key update on the server traffic key. Traffic secrets must be
   * derived.
   */
  virtual uint32_t serverKeyUpdate();

  /**
   * Retreive a secret from the scheduler. Must be in the appropriate state.
   */
  virtual DerivedSecret getSecret(EarlySecrets s, folly::ByteRange transcript)
      const;
  virtual DerivedSecret getSecret(
      HandshakeSecrets s,
      folly::ByteRange transcript) const;
  virtual DerivedSecret getSecret(MasterSecrets s, folly::ByteRange transcript)
      const;
  virtual DerivedSecret getSecret(AppTrafficSecrets s) const;

  /**
   * Derive a traffic key and iv from a traffic secret.
   */
  virtual TrafficKey getTrafficKey(
      folly::ByteRange trafficSecret,
      size_t keyLength,
      size_t ivLength) const;

  /**
   * Derive a traffic key and iv from a traffic secret with the label supplied.
   */
  virtual TrafficKey getTrafficKeyWithLabel(
      folly::ByteRange trafficSecret,
      folly::StringPiece keyLabel,
      folly::StringPiece ivLabel,
      size_t keyLength,
      size_t ivLength) const;

  /**
   * Derive a resumption secret with a particular ticket nonce. Does not require
   * being in master secret state.
   */
  virtual Buf getResumptionSecret(
      folly::ByteRange resumptionMasterSecret,
      folly::ByteRange ticketNonce) const;

 private:
  struct EarlySecret {
    std::vector<uint8_t> secret;
  };
  struct HandshakeSecret {
    std::vector<uint8_t> secret;
  };
  struct MasterSecret {
    std::vector<uint8_t> secret;
  };
  struct AppTrafficSecret {
    std::vector<uint8_t> client;
    uint32_t clientGeneration{0};
    std::vector<uint8_t> server;
    uint32_t serverGeneration{0};
  };

  folly::Optional<boost::variant<EarlySecret, HandshakeSecret, MasterSecret>>
      secret_;
  folly::Optional<AppTrafficSecret> appTrafficSecret_;

  std::unique_ptr<KeyDerivation> deriver_;
};
} // namespace fizz
