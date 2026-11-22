#include "mainwindow.h"
#include "translator.h"
#include "compiler.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QTableWidgetItem>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow), executionProcess(new QProcess(this))
{
    ui->setupUi(this);
    connect(ui->translateButton, &QPushButton::clicked, this, &MainWindow::translate);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::clearEditors);
    connect(ui->exampleButton, &QPushButton::clicked, this, &MainWindow::loadExample);
    connect(ui->factorialButton, &QPushButton::clicked, this, &MainWindow::loadFactorialExample);
    connect(ui->primeButton, &QPushButton::clicked, this, &MainWindow::loadPrimeExample);
    connect(ui->fibonacciButton, &QPushButton::clicked, this, &MainWindow::loadFibonacciExample);
    connect(ui->copyButton, &QPushButton::clicked, this, &MainWindow::copyOutput);
    connect(ui->executeButton, &QPushButton::clicked, this, &MainWindow::executeOutput);
    connect(executionProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        ui->consoleOutput->appendPlainText(QString::fromUtf8(executionProcess->readAllStandardOutput()));
    });
    connect(executionProcess, &QProcess::readyReadStandardError, this, [this]() {
        ui->consoleOutput->appendPlainText(QString::fromUtf8(executionProcess->readAllStandardError()));
    });
    connect(executionProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, &MainWindow::processFinished);
}
MainWindow::~MainWindow() { delete ui; }

void MainWindow::translate()
{
    const CompilerResult analysis = analyzePython(ui->pythonEditor->toPlainText());
    const TranslationResult r = translatePythonToCSharp(ui->pythonEditor->toPlainText());
    ui->csharpEditor->setPlainText(r.code);
    ui->tokenTable->setRowCount(analysis.tokens.size());
    for(int i=0;i<analysis.tokens.size();++i){const auto&t=analysis.tokens.at(i);ui->tokenTable->setItem(i,0,new QTableWidgetItem(t.lexeme));ui->tokenTable->setItem(i,1,new QTableWidgetItem(t.type));ui->tokenTable->setItem(i,2,new QTableWidgetItem(QString::number(t.line)));}
    ui->symbolTable->setRowCount(analysis.symbols.size());
    for(int i=0;i<analysis.symbols.size();++i){const auto&s=analysis.symbols.at(i);ui->symbolTable->setItem(i,0,new QTableWidgetItem(s.name));ui->symbolTable->setItem(i,1,new QTableWidgetItem(s.type));ui->symbolTable->setItem(i,2,new QTableWidgetItem(s.category));ui->symbolTable->setItem(i,3,new QTableWidgetItem(s.scope));ui->symbolTable->setItem(i,4,new QTableWidgetItem(QString::number(s.line)));}
    ui->intermediateOutput->setPlainText(analysis.intermediateCode);
    QStringList report=r.phases; report<<""<<"Errores lexicos:"<<analysis.lexicalErrors<<"Errores sintacticos:"<<analysis.syntaxErrors<<"Errores semanticos:"<<analysis.semanticErrors;
    ui->phaseOutput->setPlainText(report.join('\n'));
    const bool ok=r.success&&analysis.success(); ui->executeButton->setEnabled(ok);
    if(ok){ui->statusLabel->setText("Compilacion completada: todas las fases validas");ui->statusLabel->setStyleSheet("color:#137333;font-weight:bold");}
    else{ui->statusLabel->setText("Se encontraron errores; ejecucion bloqueada");ui->statusLabel->setStyleSheet("color:#b42318;font-weight:bold");QStringList e=r.errors+analysis.lexicalErrors+analysis.syntaxErrors+analysis.semanticErrors;QMessageBox::warning(this,"Errores del compilador",e.join('\n'));}
}
void MainWindow::clearEditors() { ui->pythonEditor->clear(); ui->csharpEditor->clear(); ui->phaseOutput->clear(); ui->consoleOutput->clear();ui->intermediateOutput->clear();ui->tokenTable->setRowCount(0);ui->symbolTable->setRowCount(0);ui->executeButton->setEnabled(false); ui->statusLabel->setText("Listo"); ui->statusLabel->setStyleSheet({}); }
void MainWindow::copyOutput() { QApplication::clipboard()->setText(ui->csharpEditor->toPlainText()); ui->statusLabel->setText("Codigo C# copiado"); }

void MainWindow::executeOutput()
{
    if (executionProcess->state() != QProcess::NotRunning) {
        QMessageBox::information(this, "Ejecucion", "Ya hay un programa en ejecucion.");
        return;
    }
    const QString code = ui->csharpEditor->toPlainText().trimmed();
    if (code.isEmpty()) {
        QMessageBox::information(this, "Ejecucion", "Primero traduzca un programa Python.");
        return;
    }

    const QString runtimeDir = QDir::tempPath() + "/TraductorPythonCSharpRuntime";
    QDir().mkpath(runtimeDir);
    QFile source(runtimeDir + "/Program.cs");
    QFile project(runtimeDir + "/Runtime.csproj");
    if (!source.open(QIODevice::WriteOnly | QIODevice::Text) || !project.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Ejecucion", "No fue posible crear los archivos temporales.");
        return;
    }
    QTextStream(&source) << code;
    QTextStream(&project) << "<Project Sdk=\"Microsoft.NET.Sdk\"><PropertyGroup><OutputType>Exe</OutputType><TargetFramework>net10.0</TargetFramework><ImplicitUsings>enable</ImplicitUsings></PropertyGroup></Project>";
    source.close();
    project.close();

    ui->consoleOutput->clear();
    ui->consoleOutput->appendPlainText("> Compilando y ejecutando el codigo C#...\n");
    ui->executeButton->setEnabled(false);
    ui->statusLabel->setText("Ejecutando C#");
    executionProcess->setWorkingDirectory(runtimeDir);
    executionProcess->start("dotnet", {"run", "--project", runtimeDir + "/Runtime.csproj"});
    if (!executionProcess->waitForStarted(5000)) {
        ui->executeButton->setEnabled(true);
        ui->consoleOutput->appendPlainText("No se encontro el SDK de .NET. Instale .NET SDK 10 o superior.");
    }
}

void MainWindow::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    ui->executeButton->setEnabled(true);
    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        ui->consoleOutput->appendPlainText("\n> Ejecucion finalizada correctamente.");
        ui->statusLabel->setText("Ejecucion completada");
        ui->statusLabel->setStyleSheet("color:#137333;font-weight:bold");
    } else {
        ui->consoleOutput->appendPlainText(QString("\n> La compilacion o ejecucion termino con codigo %1.").arg(exitCode));
        ui->statusLabel->setText("Error de ejecucion");
        ui->statusLabel->setStyleSheet("color:#b42318;font-weight:bold");
    }
}
void MainWindow::loadExample()
{
    ui->pythonEditor->setPlainText(
        "nombre = \"Elvy\"\n"
        "edad = 22\n"
        "promedio = 89.5\n"
        "aprobado = True\n\n"
        "if edad >= 18:\n"
        "    print(nombre)\n"
        "    print(\"Mayor de edad\")\n"
        "else:\n"
        "    print(\"Menor de edad\")\n\n"
        "for i in range(0, 3):\n"
        "    print(i)\n");
}

void MainWindow::loadFactorialExample()
{
    ui->pythonEditor->setPlainText(
        "def factorial(n):\n"
        "    if n <= 1:\n"
        "        return 1\n"
        "    return n * factorial(n - 1)\n"
        "\n"
        "if __name__ == \"__main__\":\n"
        "    numero = 97\n"
        "    print(\"Factorial de 97:\")\n"
        "    print(factorial(numero))\n");
    ui->statusLabel->setText("Ejemplo de factorial cargado");
}

void MainWindow::loadPrimeExample()
{
    ui->pythonEditor->setPlainText(
        "def es_primo(n):\n"
        "    if n < 2:\n"
        "        return False\n"
        "    divisor = 2\n"
        "    while divisor < n:\n"
        "        if n % divisor == 0:\n"
        "            return False\n"
        "        divisor = divisor + 1\n"
        "    return True\n"
        "\n"
        "if __name__ == \"__main__\":\n"
        "    print(\"Numeros primos del 2 al 29:\")\n"
        "    for numero in range(2, 30):\n"
        "        if es_primo(numero):\n"
        "            print(numero)\n");
    ui->statusLabel->setText("Ejemplo de numeros primos cargado");
}

void MainWindow::loadFibonacciExample()
{
    ui->pythonEditor->setPlainText(
        "def fibonacci(n):\n"
        "    if n <= 1:\n"
        "        return n\n"
        "    return fibonacci(n - 1) + fibonacci(n - 2)\n"
        "\n"
        "if __name__ == \"__main__\":\n"
        "    print(\"Serie de Fibonacci:\")\n"
        "    for i in range(0, 10):\n"
        "        print(fibonacci(i))\n");
    ui->statusLabel->setText("Ejemplo de Fibonacci cargado");
}
