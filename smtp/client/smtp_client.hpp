#pragma once

#include "../common/socket_connection.hpp"
#include "../message/email.hpp"

void SendEmail(Socket& socket, const EmailMessage& message);
