/* SPDX-License-Identifier: GPL-3.0-only */
#pragma once

/* Microsoft public-client profile, fixed at compile time for the whole
 * process. Device-code clients have no secret. Switching profile requires a
 * complete new device sign-in: saved refresh credentials are bound to this
 * client ID and scope and are never carried from one profile to another. */

/* Original XCloud4 public registration, retained as an alternate upstream profile. */
#define X4_AUTH_OWN_CLIENT_ID "f9ac8684-1032-4131-bb47-d2f58da9bb93"
/* Public client ID used by GreenVita (Day-OS green-vita,
 * src/api_xbox/auth.rs), used for compatibility; this public identifier is not a client secret. */
#define X4_AUTH_REFERENCE_CLIENT_ID "1f907974-e22b-4810-a9de-d9647380c97e"

#ifndef X4_AUTH_USE_REFERENCE
#define X4_AUTH_USE_REFERENCE 1 /* compatibility profile */
#endif

#if X4_AUTH_USE_REFERENCE != 0 && X4_AUTH_USE_REFERENCE != 1
#error "X4_AUTH_USE_REFERENCE must be 0 or 1"
#endif

#if X4_AUTH_USE_REFERENCE
#define X4_AUTH_CLIENT_ID X4_AUTH_REFERENCE_CLIENT_ID
#define X4_AUTH_PROFILE_NOTE "ACCESO TEMPORAL: CLIENTE DE REFERENCIA USADO POR GREENVITA."
#define X4_AUTH_PROFILE_PROMPT "AUTORIZA EN MICROSOFT. CLIENTE TEMPORAL USADO POR GREENVITA."
#else
#define X4_AUTH_CLIENT_ID X4_AUTH_OWN_CLIENT_ID
#define X4_AUTH_PROFILE_NOTE "ACCESO CON EL REGISTRO ORIGINAL DE XCLOUD4."
#define X4_AUTH_PROFILE_PROMPT "AUTORIZA NEXO X4 DESDE TU TELEFONO O PC."
#endif
