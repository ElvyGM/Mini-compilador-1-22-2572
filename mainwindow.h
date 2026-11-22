#pragma once
#include <QMainWindow>
#include <QProcess>
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
private slots:
    void translate();
    void clearEditors();
    void loadExample();
    void loadFactorialExample();
    void loadPrimeExample();
    void loadFibonacciExample();
    void copyOutput();
    void executeOutput();
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
private:
    Ui::MainWindow *ui;
    QProcess *executionProcess;
};
