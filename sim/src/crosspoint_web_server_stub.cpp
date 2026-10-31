#include "network/CrossPointWebServer.h"

CrossPointWebServer::CrossPointWebServer() = default;
CrossPointWebServer::~CrossPointWebServer() = default;

void CrossPointWebServer::begin() { running = false; }

void CrossPointWebServer::stop() { running = false; }

void CrossPointWebServer::handleClient() {}

CrossPointWebServer::WsUploadStatus CrossPointWebServer::getWsUploadStatus() const { return {}; }
