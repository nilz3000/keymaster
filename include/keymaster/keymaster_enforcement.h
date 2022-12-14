/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_LIBRARY_KEYMASTER_ENFORCEMENT_H
#define ANDROID_LIBRARY_KEYMASTER_ENFORCEMENT_H

#include <array>

#include <stdio.h>

#include <keymaster/android_keymaster_messages.h>
#include <keymaster/authorization_set.h>
#include <keymaster/keymaster_utils.h>

namespace keymaster {

typedef uint64_t km_id_t;

class KeymasterEnforcementContext {
  public:
    virtual ~KeymasterEnforcementContext() {}
    /*
     * Get current time.
     */
};

class AccessTimeMap;
class AccessCountMap;
struct HmacSharingParameters;
struct HmacSharingParametersArray;

class KeymasterEnforcement {
  public:
    /**
     * Construct a KeymasterEnforcement.
     */
    KeymasterEnforcement(uint32_t max_access_time_map_size, uint32_t max_access_count_map_size);
    virtual ~KeymasterEnforcement();

    /**
     * Iterates through the authorization set and returns the corresponding keymaster error. Will
     * return KM_ERROR_OK if all criteria is met for the given purpose in the authorization set with
     * the given operation params and handle. Used for encrypt, decrypt sign, and verify.
     */
    keymaster_error_t AuthorizeOperation(const keymaster_purpose_t purpose, const km_id_t keyid,
                                         const AuthProxy& auth_set,
                                         const AuthorizationSet& operation_params,
                                         keymaster_operation_handle_t op_handle,
                                         bool is_begin_operation);

    /**
     * Iterates through the authorization set and returns the corresponding keymaster error. Will
     * return KM_ERROR_OK if all criteria is met for the given purpose in the authorization set with
     * the given operation params. Used for encrypt, decrypt sign, and verify.
     */
    keymaster_error_t AuthorizeBegin(const keymaster_purpose_t purpose, const km_id_t keyid,
                                     const AuthProxy& auth_set,
                                     const AuthorizationSet& operation_params);

    /**
     * Iterates through the authorization set and returns the corresponding keymaster error. Will
     * return KM_ERROR_OK if all criteria is met for the given purpose in the authorization set with
     * the given operation params and handle. Used for encrypt, decrypt sign, and verify.
     */
    keymaster_error_t AuthorizeUpdate(const AuthProxy& auth_set,
                                      const AuthorizationSet& operation_params,
                                      keymaster_operation_handle_t op_handle) {
        return AuthorizeUpdateOrFinish(auth_set, operation_params, op_handle);
    }

    /**
     * Iterates through the authorization set and returns the corresponding keymaster error. Will
     * return KM_ERROR_OK if all criteria is met for the given purpose in the authorization set with
     * the given operation params and handle. Used for encrypt, decrypt sign, and verify.
     */
    keymaster_error_t AuthorizeFinish(const AuthProxy& auth_set,
                                      const AuthorizationSet& operation_params,
                                      keymaster_operation_handle_t op_handle) {
        return AuthorizeUpdateOrFinish(auth_set, operation_params, op_handle);
    }

    //
    // Methods that must be implemented by subclasses
    //
    // The time-related methods address the fact that different enforcement contexts may have
    // different time-related capabilities.  In particular:
    //
    // - They may or may not be able to check dates against real-world clocks.
    //
    // - They may or may not be able to check timestampls against authentication trustlets (minters
    //   of hw_auth_token_t structs).
    //
    // - They must have some time source for relative times, but may not be able to provide more
    //   than reliability and monotonicity.

    /*
     * Returns true if the specified activation date has passed, or if activation cannot be
     * enforced.
     */
    virtual bool activation_date_valid(uint64_t activation_date) const = 0;

    /*
     * Returns true if the specified expiration date has passed.  Returns false if it has not, or if
     * expiration cannot be enforced.
     */
    virtual bool expiration_date_passed(uint64_t expiration_date) const = 0;

    /*
     * Returns true if the specified auth_token is older than the specified timeout.
     */
    virtual bool auth_token_timed_out(const hw_auth_token_t& token, uint32_t timeout) const = 0;

    /*
     * Get current time in milliseconds from some starting point.  This value is used to compute
     * relative times between events.  It must be monotonically increasing, and must not skip or
     * lag.  It need not have any relation to any external time standard (other than the duration of
     * "second").
     *
     * On Linux systems, it's recommended to use clock_gettime(CLOCK_BOOTTIME, ...) to implement
     * this method.  On non-Linux POSIX systems, CLOCK_MONOTONIC is good, assuming the device does
     * not suspend.
     */
    virtual uint64_t get_current_time_ms() const = 0;

    /*
     * Get whether or not we're in early boot.  See early_boot_ended() below.
     */
    bool in_early_boot() const { return in_early_boot_; }

    /*
     * Get current time in seconds from some starting point.  This value is used to compute relative
     * times between events.  It must be monotonically increasing, and must not skip or lag.  It
     * need not have any relation to any external time standard (other than the duration of
     * "second").
     */
    uint32_t get_current_time() const {
        return static_cast<uint32_t>(get_current_time_ms() / 1000);  // Will wrap every 136 years
    }

    /*
     * Returns the security level of this implementation.
     */
    virtual keymaster_security_level_t SecurityLevel() const = 0;

    /*
     * Returns true if the specified auth_token has a valid signature, or if signature validation is
     * not available.
     */
    virtual bool ValidateTokenSignature(const hw_auth_token_t& token) const = 0;

    /**
     * Get the sharing parameters used to negotiate a shared HMAC key among multiple parties.
     */
    virtual keymaster_error_t GetHmacSharingParameters(HmacSharingParameters* params) = 0;

    /**
     * Compute an HMAC key shared among multiple parties.
     */
    virtual keymaster_error_t ComputeSharedHmac(const HmacSharingParametersArray& params_array,
                                                KeymasterBlob* sharingCheck) = 0;

    /**
     * Verify authorizations for another Keymaster instance.
     */
    virtual VerifyAuthorizationResponse
    VerifyAuthorization(const VerifyAuthorizationRequest& request) = 0;

    /**
     * Generate TimestampToken for secure clock instance.
     */
    virtual keymaster_error_t GenerateTimestampToken(TimestampToken* token);

    /**
     * Compute an HMAC using the auth token HMAC key.
     *
     * Note: Use with caution.  MACed data must contain enough structure that it's unambiguous.
     */
    virtual KmErrorOr<std::array<uint8_t, 32>>
    ComputeHmac(const std::vector<uint8_t>& /* data_to_mac */) const {
        return KM_ERROR_UNIMPLEMENTED;
    }

    /**
     * Creates a key ID for use in subsequent calls to AuthorizeOperation.  AndroidKeymaster
     * uses this method for creating key IDs. The generated id must be stable in that the same
     * key_blob bits yield the same keyid.
     *
     * Returns false if an error in the crypto library prevents creation of an ID.
     */
    virtual bool CreateKeyId(const keymaster_key_blob_t& key_blob, km_id_t* keyid) const = 0;

    /*
     * Inform the KeymasterEnforcement object that early boot stage has ended.
     */
    void early_boot_ended() { in_early_boot_ = false; }

    /*
     * Inform the KeymasterEnforcement object that the device is locked, so it knows not to permit
     * UNLOCKED_DEVICE_REQUIRED keys to be used until a fresh (later than "now") auth token is
     * provided.  If password_only is true, the fresh auth token must additionally be a password
     * auth token.
     */
    void device_locked(bool password_only) {
        device_locked_at_ = get_current_time_ms();
        password_unlock_only_ = password_only;
    }

  private:
    keymaster_error_t AuthorizeUpdateOrFinish(const AuthProxy& auth_set,
                                              const AuthorizationSet& operation_params,
                                              keymaster_operation_handle_t op_handle);

    bool MinTimeBetweenOpsPassed(uint32_t min_time_between, const km_id_t keyid);
    bool MaxUsesPerBootNotExceeded(const km_id_t keyid, uint32_t max_uses);
    bool GetAndValidateAuthToken(const AuthorizationSet& operation_params,
                                 const hw_auth_token_t** token, uint32_t* token_auth_type) const;
    bool AuthTokenMatches(const AuthProxy& auth_set, const AuthorizationSet& operation_params,
                          const uint64_t user_secure_id, const int auth_type_index,
                          const int auth_timeout_index,
                          const keymaster_operation_handle_t op_handle,
                          bool is_begin_operation) const;

    AccessTimeMap* access_time_map_;
    AccessCountMap* access_count_map_;
    bool in_early_boot_ = true;
    uint64_t device_locked_at_ = 0;
    bool password_unlock_only_ = false;
};

}; /* namespace keymaster */

#endif  // ANDROID_LIBRARY_KEYMASTER_ENFORCEMENT_H
