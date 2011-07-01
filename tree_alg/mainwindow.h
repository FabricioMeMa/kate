#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextCursor>
#include "foldingtree.h"
#include <QFileDialog>

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QTextCursor *cursor;
    void print(int position);
    FoldingTree *folding_tree;

    // Start of Test Methods
    void testMergeDriver();

    int histLimit;
    static QString defaultHistoryFile;

    void saveHistoryToFile(QString fileName = defaultHistoryFile);
    void displayTree();
    void displayStack();
    void check();



private slots:
    void on_pushButton_9_released();
    void on_pushButton_released();
    void on_checkBox_4_stateChanged(int );
    void on_pushButton_8_released();
    void on_spinBox_valueChanged(int );
    void on_pushButton_7_released();
    void on_checkBox_3_stateChanged(int );
    void on_checkBox_2_stateChanged(int );
    void on_pushButton_6_released();
    void on_checkBox_stateChanged(int newState);
    void on_pushButton_5_released();
    void on_pushButton_4_released();
    void on_pushButton_3_released();
    void on_pushButton_2_released();
};

#endif // MAINWINDOW_H
