/*
 * Copyright (C) 2019 Ashar Khan <ashar786khan@gmail.com>
 *
 * This file is part of CPEditor.
 *
 * CPEditor is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * I will not be responsible if CPEditor behaves in unexpected way and
 * causes your ratings to go down and or loose any important contest.
 *
 * Believe Software is "Software" and it isn't not immune to bugs.
 *
 */

#include <MessageLogger.hpp>
#include <QJsonDocument>
#include <UpdateNotifier.hpp>

namespace Telemetry {
UpdateNotifier::UpdateNotifier(bool useBeta) {
  manager = new QNetworkAccessManager();
  QObject::connect(manager, SIGNAL(finished(QNetworkReply*)), this,
                   SLOT(managerFinished(QNetworkReply*)));
  beta = useBeta;
}
UpdateNotifier::~UpdateNotifier() {
  delete manager;
}
void UpdateNotifier::setBeta(bool value) {
  beta = value;
}

void UpdateNotifier::checkUpdate() {
  request.setUrl(QUrl(
      "https://api.github.com/repos/coder3101/cp-editor2/releases/latest"));
  manager->get(request);
}

QString UpdateNotifier::currentVersionStr() {
  std::string version = std::to_string(APP_VERSION_MAJOR) + "." +
                        std::to_string(APP_VERSION_MINOR) + "." +
                        std::to_string(APP_VERSION_PATCH);
  return QString::fromStdString(version);
}
void UpdateNotifier::managerFinished(QNetworkReply* reply) {
  if (reply->error()) {
    qDebug() << reply->errorString();
    return;
  }
  QString jsonReply = reply->readAll();

  QJsonDocument doc = QJsonDocument::fromJson(jsonReply.toLocal8Bit());

  QString latestRelease = doc["tag_name"].toString();
  bool isBeta = doc["prerelease"].toBool();
  QString downloadUrl = doc["html_url"].toString();

  QList<QString> latestSplit = latestRelease.split(".");
  QList<QString> currenSplit = currentVersionStr().split(".");

  bool isUpdateAvailable = latestRelease > currentVersionStr();
  qDebug() << "Update is : " << isUpdateAvailable << "\n";

  bool showMessage = (isUpdateAvailable && (beta || !isBeta));
  if (showMessage) {
    Log::MessageLogger::info(
        "Updater", "A new update " + latestRelease.toStdString() +
                       " is available. <a href = " + downloadUrl.toStdString() +
                       ">Please Download" + "</a>");
  }
}
}  // namespace Telemetry
