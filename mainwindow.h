#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSql>
#include <QDebug>
#include <QMessageBox>
#include <QFileDialog> // Dosya seçmek için lazım

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnIhbarKaydet_clicked();   // İhbar kaydetme
    void on_btnHayvanKaydet_clicked();  // Hayvan kaydetme (YENİ)
    void on_btnFotoSec_clicked();       // Fotoğraf seçme (YENİ)

    void on_btnArsivListele_clicked();

    void on_btnIhbarSil_clicked();

    void on_btnArsivSil_clicked();

    void on_tabloArsiv_cellDoubleClicked(int row, int column);

    void on_btnExcelAktar_clicked();

    void on_chkCipVar_toggled(bool checked);

    void on_chkCikis_toggled(bool checked);

    void on_btnTedaviEkle_clicked();

    void on_cmbDurum_currentTextChanged(const QString &arg1);

private:
    Ui::MainWindow *ui;
    QSqlDatabase db;
    QString secilenResimYolu; // Fotoğrafın adresini burada tutacağız

    int duzenlenecekKayitID = -1;

    QWidget *tabDashboard; // Yeni sekmemiz
    QLabel *lblToplamHayvan;
    QLabel *lblSahiplendirilen;
    QLabel *lblTedaviGoren;
    QLabel *lblBugunGelen;

    void dashboardOlustur();   // Ekranı çizen fonksiyon
    void istatistikleriGuncelle();

    void veritabaniKur();
    void ihbarlariListele();
    void ihbarlariComboyaDoldur(); // İhbarları listeye çeken sihirli fonksiyon
    void veritabaniYedekle();
};
#endif // MAINWINDOW_H
