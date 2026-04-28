#include "network/WebDAVHandler.h"

bool WebDAVHandler::canHandle(WebServer&, HTTPMethod, const String&) { return false; }

bool WebDAVHandler::canRaw(WebServer&, const String&) { return false; }

void WebDAVHandler::raw(WebServer&, const String&, HTTPRaw&) {}

bool WebDAVHandler::handle(WebServer&, HTTPMethod, const String&) { return false; }
