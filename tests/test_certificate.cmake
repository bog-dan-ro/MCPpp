# Copyright (c) 2026 BogDan Vatra
# SPDX-License-Identifier: MIT

file(MAKE_DIRECTORY "${CERTIFICATE_DIR}")
execute_process(COMMAND "${OPENSSL_EXECUTABLE}" req -x509 -newkey rsa:2048 -noenc -sha256
  -days 2 -subj /CN=localhost -addext subjectAltName=DNS:localhost,IP:127.0.0.1
  -keyout "${CERTIFICATE_DIR}/key.pem" -out "${CERTIFICATE_DIR}/cert.pem"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE errors)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Could not generate temporary TLS test certificate: ${errors}")
endif()
