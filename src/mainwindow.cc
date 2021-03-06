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


#include "mainwindow.hpp"

#include <Core.hpp>
#include <MessageLogger.hpp>
#include <QCXXHighlighter>
#include <QFileDialog>
#include <QFontDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPythonCompleter>
#include <QPythonHighlighter>
#include <QSyntaxStyle>
#include <QTextStream>
#include <QThread>
#include <QTimer>

#include "../ui/ui_mainwindow.h"

// ***************************** RAII  ****************************
MainWindow::MainWindow(QString filePath, QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
  ui->setupUi(this);
  setEditor();
  setLogger();
  setSettingsManager();
  restoreSettings();
  runEditorDiagonistics();
  setupCore();
  runner->removeExecutable();
  launchSession();
  checkUpdates();

  if(!filePath.isEmpty()){
      openFile = new QFile(filePath);
      openFile->open(QIODevice::ReadWrite | QFile::Text);
      if(openFile->isOpen()){
          this->window()->setWindowTitle("CP Editor : "+ openFile->fileName());
          editor->setPlainText(openFile->readAll());
      }
      else{
          Log::MessageLogger::warn("Loader", "The filepath was not loaded. Read/Write permission missing");
          openFile->close();
          delete openFile;
          openFile = nullptr;
      }
  }
}

MainWindow::~MainWindow() {
  delete ui;
  delete editor;
  delete setting;
  if (openFile != nullptr && openFile->isOpen())
    openFile->close();
  delete openFile;
  delete inputReader;
  delete outputReader;
  delete outputWriter;
  delete formatter;
  delete compiler;
  delete runner;
  delete saveTimer;
  delete updater;
}

// ************************* RAII HELPER *****************************

void MainWindow::setEditor() {
  editor = new QCodeEditor();
  editor->setMinimumWidth(700);
  editor->setMinimumHeight(400);

  editor->setSyntaxStyle(QSyntaxStyle::defaultStyle());  // default is white
  editor->setHighlighter(new QCXXHighlighter);
  editor->setAutoIndentation(true);
  editor->setAutoParentheses(true);
  editor->setWordWrapMode(QTextOption::NoWrap);

  ui->verticalLayout_8->addWidget(editor);

  ui->in1->setWordWrapMode(QTextOption::NoWrap);
  ui->in2->setWordWrapMode(QTextOption::NoWrap);
  ui->in3->setWordWrapMode(QTextOption::NoWrap);
  ui->out1->setWordWrapMode(QTextOption::NoWrap);
  ui->out2->setWordWrapMode(QTextOption::NoWrap);
  ui->out3->setWordWrapMode(QTextOption::NoWrap);

  ui->expected->hide();

  saveTimer = new QTimer();
  saveTimer->setSingleShot(false);
  saveTimer->setInterval(10000);  // every 10 sec save
}

void MainWindow::setLogger() {
  Log::MessageLogger::setContainer(ui->compiler_edit);
  ui->compiler_edit->setWordWrapMode(QTextOption::NoWrap);
  ui->compiler_edit->setReadOnly(true);
}

void MainWindow::setSettingsManager() {
  setting = new Settings::SettingManager();
}

void MainWindow::saveSettings() {
  setting->setDarkTheme(ui->actionDark_Theme->isChecked());
  setting->setWrapText(ui->actionWrap_Text->isChecked());
  setting->setAutoIndent(ui->actionAuto_Indentation->isChecked());
  setting->setAutoParenthesis(ui->actionAuto_Parenthesis->isChecked());
  setting->setFont(editor->font().toString().toStdString());
  setting->setAutoSave(ui->actionAuto_Save->isChecked());
}

void MainWindow::checkUpdates() {
  if (updater != nullptr)
    updater->checkUpdate();
}

void MainWindow::restoreSettings() {
  if (setting->isDarkTheme()) {
    ui->actionDark_Theme->setChecked(true);
    on_actionDark_Theme_triggered(true);
  }
  if (setting->isWrapText()) {
    ui->actionWrap_Text->setChecked(true);
    on_actionWrap_Text_triggered(true);
  }
  if (!setting->isAutoIndent()) {
    ui->actionAuto_Indentation->setChecked(false);
    on_actionAuto_Indentation_triggered(false);
  }
  if (!setting->isAutoParenthesis()) {
    ui->actionAuto_Parenthesis->setChecked(false);
    on_actionAuto_Parenthesis_triggered(false);
  }
  auto lang = setting->getDefaultLang();

  if (lang == "Java") {
    ui->actionC_C->setChecked(false);
    ui->actionJava->setChecked(true);
    ui->actionPython->setChecked(false);
    editor->setHighlighter(new QCXXHighlighter);
    editor->setCompleter(nullptr);
    // TODO(Add Java Highlighter)
    language = "Java";

  } else if (lang == "Python") {
    ui->actionC_C->setChecked(false);
    ui->actionJava->setChecked(false);
    ui->actionPython->setChecked(true);

    editor->setCompleter(new QPythonCompleter);
    editor->setHighlighter(new QPythonHighlighter);
    language = "Python";
  } else {
    if (lang != "Cpp")
      Log::MessageLogger::warn(
          "Settings",
          "File was distrubed, Cannot load default language, fall back to C++");
    ui->actionC_C->setChecked(true);
    ui->actionJava->setChecked(false);
    ui->actionPython->setChecked(false);

    // TODO(Add C++ Completer)
    editor->setHighlighter(new QCXXHighlighter);
    editor->setCompleter(nullptr);
    language = "Cpp";
  }

  if (!setting->getFont().empty()) {
    auto font = QFont();
    if (font.fromString(QString::fromStdString(setting->getFont()))) {
      editor->setFont(font);
    }
  }
  if (setting->isAutoSave()) {
    ui->actionAuto_Save->setChecked(true);
    on_actionAuto_Save_triggered(true);
  }
}

void MainWindow::runEditorDiagonistics() {
  if (!Core::Formatter::check(
          QString::fromStdString(setting->getFormatCommand()))) {
    Log::MessageLogger::error(
        "Formatter",
        "Format feature will not work as your format command is not valid");
  }
  if (!Core::Compiler::check(
          QString::fromStdString(setting->getCompileCommand()))) {
    Log::MessageLogger::error("Compiler",
                              "Compiler will not work, Change Compile command "
                              "and make sure it is in path");
  }
  if (setting->getTemplatePath().size() != 0 &&
      !QFile::exists(QString::fromStdString(setting->getTemplatePath()))) {
    Log::MessageLogger::error(
        "Template",
        "The specified template file does not exists or is not readable");
  }
}

void MainWindow::setupCore() {
  formatter =
      new Core::Formatter(QString::fromStdString(setting->getFormatCommand()));
  outputReader = new Core::IO::OutputReader(ui->out1, ui->out2, ui->out3);
  outputWriter = new Core::IO::OutputWriter(ui->out1, ui->out2, ui->out3);
  inputReader = new Core::IO::InputReader(ui->in1, ui->in2, ui->in3);
  compiler =
      new Core::Compiler(QString::fromStdString(setting->getCompileCommand()));
  runner =
      new Core::Runner(QString::fromStdString(setting->getRunCommand()),
                       QString::fromStdString(setting->getCompileCommand()),
                       QString::fromStdString(setting->getPrependRunCommand()));

  updater = new Telemetry::UpdateNotifier(setting->isBeta());

  QObject::connect(runner, SIGNAL(firstExecutionFinished(QString, QString)),
                   this, SLOT(firstExecutionFinished(QString, QString)));

  QObject::connect(runner, SIGNAL(secondExecutionFinished(QString, QString)),
                   this, SLOT(secondExecutionFinished(QString, QString)));

  QObject::connect(runner, SIGNAL(thirdExecutionFinished(QString, QString)),
                   this, SLOT(thirdExecutionFinished(QString, QString)));

  QObject::connect(saveTimer, SIGNAL(timeout()), this,
                   SLOT(onSaveTimerElapsed()));
}

void MainWindow::launchSession() {
  if (openFile != nullptr) {
    if (openFile->isOpen()) {
      openFile->resize(0);
      openFile->write(editor->toPlainText().toStdString().c_str());
      openFile->close();
    }
    delete openFile;
    openFile = nullptr;
  }

  if (setting->getTemplatePath().size() == 0)
    return;
  if (QFile::exists(QString::fromStdString(setting->getTemplatePath()))) {
    QFile f(QString::fromStdString(setting->getTemplatePath()));
    f.open(QIODevice::ReadOnly | QFile::Text);
    editor->setPlainText(f.readAll());
    Log::MessageLogger::info("Template", "Template was successfully loaded");
  } else {
    Log::MessageLogger::error("Template",
                              "Template could not be loaded from the file " +
                                  setting->getTemplatePath());
  }

  this->window()->setWindowTitle("CP Editor: Temporary buffer");
  ui->in1->clear();
  ui->in2->clear();
  ui->in3->clear();

  ui->out1->clear();
  ui->out2->clear();
  ui->out3->clear();
}

// ******************* STATUS::ACTIONS FILE **************************
void MainWindow::on_actionNew_triggered() {
  auto res =
      QMessageBox::question(this, "End Session?",
                            "Are you sure you want to start a new session. All "
                            "your code and binaries will be erased?",
                            QMessageBox::Yes | QMessageBox::No);
  if (res == QMessageBox::No)
    return;

  editor->clear();
  runner->removeExecutable();
  launchSession();
}
void MainWindow::on_actionOpen_triggered() {
  auto fileName = QFileDialog::getOpenFileName(
      this, tr("Open File"), "",
      "Source Files (*.cpp *.hpp *.h *.cc *.cxx *.c *.py *.py3 *.java)");
  if (fileName.isEmpty())
    return;
  QFile* newFile = new QFile(fileName);
  newFile->open(QFile::Text | QIODevice::ReadWrite);

  if (editor->toPlainText().size() != 0) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Overwrite?", "Opening a new file will overwrite this file?",
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::No)
      return;
  }
  if (newFile->isOpen()) {
    editor->setPlainText(newFile->readAll());
    if (openFile != nullptr && openFile->isOpen())
      openFile->close();
    openFile = newFile;
    Log::MessageLogger::info("Open",
                             "Opened " + openFile->fileName().toStdString());
    this->window()->setWindowTitle("CP Editor: " + openFile->fileName());

  } else {
    Log::MessageLogger::error(
        "Open", "Cannot Open, Do I have read and write permissions?");
  }
}

void MainWindow::on_actionSave_triggered() {
  if (openFile == nullptr) {
    auto filename = QFileDialog::getSaveFileName(
        this, tr("Save File"), "",
        "Source Files (*.cpp *.hpp *.h *.cc *.cxx *.c *.py *.py3 *.java)");
    if (filename.isEmpty())
      return;

    openFile = new QFile(filename);
    openFile->open(QIODevice::ReadWrite | QFile::Text);
    if (openFile->isOpen()) {
      if(openFile->write(editor->toPlainText().toStdString().c_str()) != -1)
      Log::MessageLogger::info("Save", "Saved file : " + openFile->fileName().toStdString());
      else Log::MessageLogger::warn("Save",  "File was not saved successfully");
      this->window()->setWindowTitle("CP Editor : "+ openFile->fileName());
    }
     else {
      Log::MessageLogger::error(
          "Save", "Cannot Save file. Do I have write permission?");
    }
  } else {
    openFile->resize(0);
    openFile->write(editor->toPlainText().toStdString().c_str());
    Log::MessageLogger::info(
        "Save", "Saved with file name " + openFile->fileName().toStdString());
  }
}

void MainWindow::on_actionSave_as_triggered() {
  auto filename = QFileDialog::getSaveFileName(
      this, tr("Save as File"), "",
      "Source Files (*.cpp *.hpp *.h *.cc *.cxx *.c *.py *.py3 *.java)");
  if (filename.isEmpty())
    return;
  QFile savedFile(filename);
  savedFile.open(QIODevice::ReadWrite | QFile::Text);
  if (savedFile.isOpen()) {
    savedFile.write(editor->toPlainText().toStdString().c_str());
    Log::MessageLogger::info(
        "Save as", "Saved new file name " + savedFile.fileName().toStdString());
  } else {
    Log::MessageLogger::error(
        "Save as",
        "Cannot Save as new file, Is write permission allowed to me?");
  }
}

void MainWindow::on_actionAuto_Save_triggered(bool checked) {
  if (checked) {
    saveTimer->start();
  } else {
    saveTimer->stop();
  }
}

void MainWindow::on_actionQuit_triggered() {
  saveSettings();
  auto res = QMessageBox::question(this, "Exit?",
                                   "Are you sure you want to exit the editor?",
                                   QMessageBox::Yes | QMessageBox::No);
  if (res == QMessageBox::Yes)
    QApplication::quit();
}
// *********************** ACTIONS::EDITOR ******************************
void MainWindow::on_actionDark_Theme_triggered(bool checked) {
  if (checked) {
    QFile fullDark(":/qdarkstyle/style.qss");
    fullDark.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&fullDark);

    auto qFile = new QFile(":/styles/drakula.xml");
    qFile->open(QIODevice::ReadOnly);
    auto darkTheme = new QSyntaxStyle(this);
    darkTheme->load(qFile->readAll());

    qApp->setStyleSheet(ts.readAll());
    editor->setSyntaxStyle(darkTheme);

  } else {
    qApp->setStyleSheet("");
    editor->setSyntaxStyle(QSyntaxStyle::defaultStyle());
    auto oldbackground = editor->styleSheet();
    Log::MessageLogger::info(
        "CP Editor",
        "If theme is not set correctly. Please again change theme");
  }
}

void MainWindow::on_actionWrap_Text_triggered(bool checked) {
  if (checked)
    editor->setWordWrapMode(QTextOption::WordWrap);
  else
    editor->setWordWrapMode(QTextOption::NoWrap);
}

void MainWindow::on_actionAuto_Indentation_triggered(bool checked) {
  if (checked)
    editor->setAutoIndentation(true);
  else
    editor->setAutoIndentation(false);
}

void MainWindow::on_actionAuto_Parenthesis_triggered(bool checked) {
  if (checked)
    editor->setAutoParentheses(true);
  else
    editor->setAutoParentheses(false);
}

void MainWindow::on_actionFont_triggered() {
  bool ok;
  QFont font = QFontDialog::getFont(&ok, editor->font());
  if (ok) {
    editor->setFont(font);
  }
}

// ************************************* ACTION::ABOUT ************************

void MainWindow::on_actionBeta_Updates_triggered(bool checked) {
  if (checked) {
    updater->setBeta(true);
    updater->checkUpdate();
    setting->setBeta(true);
  } else {
    updater->setBeta(false);
    setting->setBeta(true);
  }
}
void MainWindow::on_actionAbout_triggered() {
  QMessageBox::about(
      this,
      QString::fromStdString(std::string("About CP Editor ") +
                             std::to_string(APP_VERSION_MAJOR) + "." +
                             std::to_string(APP_VERSION_MINOR) + "." +
                             std::to_string(APP_VERSION_PATCH)),
      "<p>The <b>CP Editor</b> is a competitive programmer's editor "
      "which can ease the task of compiling, testing and running a program"
      "so that you (a great programmer) can focus fully on your algorithms "
      "designs. </p>");
}

void MainWindow::on_actionAbout_Qt_triggered() {
  QApplication::aboutQt();
}
void MainWindow::on_actionReset_Settings_triggered() {
  auto res = QMessageBox::question(this, "Reset settings?",
                                   "Are you sure you want to reset the"
                                   " settings?",
                                   QMessageBox::Yes | QMessageBox::No);
  if (res == QMessageBox::Yes) {
    setting->setFormatCommand("clang-format -i");
    setting->setCompileCommands("g++ -Wall");
    setting->setRunCommand("");
    setting->setDefaultLanguage("Cpp");
    setting->setTemplatePath("");
    setting->setPrependRunCommand("");

    formatter->updateCommand(
        QString::fromStdString(setting->getFormatCommand()));
    compiler->updateCommand(
        QString::fromStdString(setting->getCompileCommand()));

    runner->updateRunCommand(QString::fromStdString(setting->getRunCommand()));
    runner->updateCompileCommand(
        QString::fromStdString(setting->getCompileCommand()));
    runner->updateRunStartCommand(QString::fromStdString(setting->getPrependRunCommand()));
    runEditorDiagonistics();

  }
}

// ********************** GLOBAL::WINDOW **********************************
void MainWindow::closeEvent(QCloseEvent* event) {
  saveSettings();
  auto res = QMessageBox::question(this, "Exit?",
                                   "Are you sure you want to exit the"
                                   " editor?",
                                   QMessageBox::Yes | QMessageBox::No);
  if (res == QMessageBox::Yes)
    event->accept();
  else
    event->ignore();
}

void MainWindow::on_compile_clicked() {
  on_actionCompile_triggered();
}

void MainWindow::on_run_clicked() {
  on_actionRun_triggered();
}

void MainWindow::on_expected_clicked(bool checked) {
  ui->out1->clear();
  ui->out2->clear();
  ui->out3->clear();
  ui->out1->setReadOnly(!checked);
  ui->out2->setReadOnly(!checked);
  ui->out3->setReadOnly(!checked);
}

void MainWindow::on_runOnly_clicked() {
  Log::MessageLogger::clear();
  inputReader->readToFile();

  ui->out1->clear();
  ui->out2->clear();
  ui->out3->clear();

  if (ui->expected->isChecked())
    outputReader->readToFile();
  runner->run(!ui->in1->toPlainText().trimmed().isEmpty(),
              !ui->in2->toPlainText().trimmed().isEmpty(),
              !ui->in3->toPlainText().trimmed().isEmpty(), language);
}

void MainWindow::on_actionDetached_Execution_triggered() {
  Log::MessageLogger::clear();
  runner->runDetached(editor, language);
}

// ************************ ACTIONS::ACTIONS ******************

void MainWindow::on_actionFormat_triggered() {
  formatter->format(editor);
}

void MainWindow::on_actionRun_triggered() {
  Log::MessageLogger::clear();
  ui->out1->clear();
  ui->out2->clear();
  ui->out3->clear();
  inputReader->readToFile();
  if (ui->expected->isChecked())
    outputReader->readToFile();
  runner->run(editor, !ui->in1->toPlainText().trimmed().isEmpty(),
              !ui->in2->toPlainText().trimmed().isEmpty(),
              !ui->in3->toPlainText().trimmed().isEmpty(), language);
}

void MainWindow::on_actionCompile_triggered() {
  Log::MessageLogger::clear();
  compiler->compile(editor, language);
}

void MainWindow::on_onlyRun_triggered() {
  on_runOnly_clicked();
}

void MainWindow::on_actionKill_Processes_triggered() {
  runner->killAll();
}

// ************************ ACTIONS::SETTINGS *************************
void MainWindow::on_actionChange_compile_command_triggered() {
  bool ok = false;
  QString text = QInputDialog::getText(
      this, "Set Compile command",
      "Enter new Compile command (source path will be appended to end of this "
      "file)",
      QLineEdit::Normal, QString::fromStdString(setting->getCompileCommand()),
      &ok);
  if (ok && !text.isEmpty()) {
    setting->setCompileCommands(text.toStdString());
    runEditorDiagonistics();
    compiler->updateCommand(
        QString::fromStdString(setting->getCompileCommand()));
    runner->updateCompileCommand(
        QString::fromStdString(setting->getCompileCommand()));
  }
}

void MainWindow::on_actionChange_run_command_triggered() {
  bool ok = false;
  QString text = QInputDialog::getText(
      this, "Set Run command arguments",
      "Enter new run command arguments (filename.out will be appended to the "
      "end of "
      "command)",
      QLineEdit::Normal, QString::fromStdString(setting->getRunCommand()), &ok);
  if (ok && !text.isEmpty()) {
    setting->setRunCommand(text.toStdString());
    runner->updateRunCommand(QString::fromStdString(setting->getRunCommand()));
  }
}

void MainWindow::on_actionChange_format_command_triggered() {
  bool ok = false;
  QString text = QInputDialog::getText(
      this, "Set Format command",
      "Enter new format command (filename will be appended to end of this "
      "command)",
      QLineEdit::Normal, QString::fromStdString(setting->getFormatCommand()),
      &ok);
  if (ok && !text.isEmpty()) {
    setting->setFormatCommand(text.toStdString());
    runEditorDiagonistics();
    formatter->updateCommand(
        QString::fromStdString(setting->getFormatCommand()));
  }
}

void MainWindow::on_actionSet_Code_Template_triggered() {
  auto filename = QFileDialog::getOpenFileName(
      this, tr("Choose template File"), "",
      "Source Files (*.cpp *.hpp *.h *.cc *.cxx *.c *.py *.py3 *.java)");
  if (filename.isEmpty())
    return;
  setting->setTemplatePath(filename.toStdString());
  runEditorDiagonistics();
  Log::MessageLogger::info(
      "Template",
      "Template path updated. It will be effective from Next Session");
}

void MainWindow::on_actionRun_Command_triggered() {
  bool ok = false;
  QString text = QInputDialog::getText(
      this, "Set Run Command",
      "Enter new run command (use only when using python or java "
      "language)",
      QLineEdit::Normal,
      QString::fromStdString(setting->getPrependRunCommand()), &ok);
  if (ok && !text.isEmpty()) {
    setting->setPrependRunCommand(text.toStdString());
    runner->updateRunStartCommand(QString::fromStdString(setting->getPrependRunCommand()));
  }
}

// ************************ SLOTS ******************************************
void MainWindow::firstExecutionFinished(QString Stdout, QString Stderr) {
  Log::MessageLogger::info("Runner[1]", "Execution for first case completed");

  if (ui->expected->isChecked()) {
    ui->out1->clear();
    outputWriter->writeFromFile(1);
    if (ui->out1->toPlainText().trimmed() == Stdout.trimmed()) {
      ui->out1->appendHtml(
          "<br><b><font color=blue>Success (GOT)</font></b><br>" +
          Stdout.replace('\n', "<br>"));
    } else {
      ui->out1->appendHtml(
          "<br><b><font color=red>Failed (GOT)</font></b><br>" +
          Stdout.replace('\n', "<br>"));
    }
  } else {
    ui->out1->clear();
    ui->out1->setPlainText(Stdout);
    if (!Stderr.isEmpty())
      ui->out1->appendHtml("<br><font color=red><b>stderr:</b><br>" +
                           Stderr.replace('\n', "<br>") + "</font><br>");
  }
}
void MainWindow::secondExecutionFinished(QString Stdout, QString Stderr) {
  Log::MessageLogger::info("Runner[2]", "Execution for second case completed");

  if (ui->expected->isChecked()) {
    ui->out2->clear();
    outputWriter->writeFromFile(2);
    if (ui->out2->toPlainText().trimmed() == Stdout.trimmed()) {
      ui->out2->appendHtml(
          "<br><b><font color=blue>Success (GOT)</font></b><br>" +
          Stdout.replace('\n', "<br>"));
    } else {
      ui->out2->appendHtml(
          "<br><b><font color=red>Failed (GOT)</font></b><br>" +
          Stdout.replace('\n', "<br>"));
    }
  } else {
    ui->out2->clear();
    ui->out2->setPlainText(Stdout);
    if (!Stderr.isEmpty())
      ui->out2->appendHtml("<br><font color=red><b>stderr:</b><br>" +
                           Stderr.replace('\n', "<br>") + "</font><br>");
  }
}

void MainWindow::thirdExecutionFinished(QString Stdout, QString Stderr) {
  Log::MessageLogger::info("Runner[3]", "Execution for third case completed");
  if (ui->expected->isChecked()) {
    ui->out3->clear();
    outputWriter->writeFromFile(3);
    if (ui->out3->toPlainText().trimmed() == Stdout.trimmed()) {
      ui->out3->appendHtml(
          "<br><b><font color=blue>Success (GOT)</font></b><br>" +
          Stdout.replace('\n', "<br>"));
    } else {
      ui->out3->appendHtml(
          "<br><b><font color=red>Failed (GOT)</font></b><br>" +
          Stdout.replace('\n', "<br>"));
    }
  } else {
    ui->out3->clear();
    ui->out3->setPlainText(Stdout);
    if (!Stderr.isEmpty())
      ui->out3->appendHtml("<br><font color=red><b>stderr:</b><br>" +
                           Stderr.replace('\n', "<br>") + "</font><br>");
  }
}

void MainWindow::onSaveTimerElapsed() {
  if (openFile != nullptr && openFile->isOpen()) {
    openFile->resize(0);
    openFile->write(editor->toPlainText().toStdString().c_str());
    Log::MessageLogger::info(
        "AutoSave",
        "AutoSaved to file : " + openFile->fileName().toStdString());
  }
}

// **************************** LANGUAGE ***********************************

void MainWindow::on_actionC_C_triggered(bool checked) {
  if (checked) {
    ui->actionC_C->setChecked(true);
    ui->actionPython->setChecked(false);
    ui->actionJava->setChecked(false);
    setting->setDefaultLanguage("Cpp");
    runner->removeExecutable();
    editor->setHighlighter(new QCXXHighlighter);
    editor->setCompleter(nullptr);
    language = "Cpp";
  }
}
void MainWindow::on_actionPython_triggered(bool checked) {
  if (checked) {
    ui->actionC_C->setChecked(false);
    ui->actionPython->setChecked(true);
    ui->actionJava->setChecked(false);
    setting->setDefaultLanguage("Python");
    runner->removeExecutable();
    editor->setHighlighter(new QPythonHighlighter);
    editor->setCompleter(new QPythonCompleter);
    language = "Python";
  }
}
void MainWindow::on_actionJava_triggered(bool checked) {
  if (checked) {
    ui->actionC_C->setChecked(false);
    ui->actionPython->setChecked(false);
    ui->actionJava->setChecked(true);
    setting->setDefaultLanguage("Java");
    runner->removeExecutable();
    editor->setHighlighter(new QCXXHighlighter);
    editor->setCompleter(nullptr);
    language = "Java";
  }
}
