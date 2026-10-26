#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QFileDialog>
#include <QDate>
#include <QDebug>
#include <QFile>
#include <QTextStream>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle(" Belediye Barınak Yönetim Sistemi");

    duzenlenecekKayitID = -1; // Düzenleme modunu sıfırla

    veritabaniKur();

    // Padok Numaralarını Otomatik Doldur (1'den 50'ye kadar)
    ui->cmbPadok->clear();
    for(int i=1; i<=50; i++) {
        ui->cmbPadok->addItem(QString::number(i));
    }

    // Tedavi Türlerini Kodla da Doldurabiliriz (Tasarımda eklemediysen buradan gelir)
    if(ui->cmbTedaviTur->count() == 0) {
        ui->cmbTedaviTur->addItems({"Genel Muayene", "Aşı Uygulaması", "Parazit İlacı", "Ameliyat", "İlaç Başlangıcı", "İlaç Bitişi", "Kontrol", "Transfer"});
    }

    // Tablo Ayarları
    ui->tabloIhbarlar->setColumnCount(3);
    ui->tabloIhbarlar->setHorizontalHeaderLabels({"No", "İhbar Sahibi", "Adres"});
    ui->tabloIhbarlar->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    ihbarlariListele();
    ihbarlariComboyaDoldur();

    dashboardOlustur();      // Kutuları çiz
    istatistikleriGuncelle();
}

MainWindow::~MainWindow()
{
    veritabaniYedekle();

    delete ui;
}

void MainWindow::veritabaniKur() {
    db = QSqlDatabase::addDatabase("QSQLITE");
    // Yeni sütunlar için v3 veritabanını kullanıyoruz
    db.setDatabaseName("barinak_v3.db");

    if (!db.open()) {
        QMessageBox::critical(this, "Hata", "Veritabanı açılamadı!");
        return;
    }

    QSqlQuery sorgu;
    sorgu.exec("CREATE TABLE IF NOT EXISTS ihbarlar (id INTEGER PRIMARY KEY AUTOINCREMENT, isim TEXT, adres TEXT)");

    // Medikal notlar ve Padok sütunları eklenmiş Tablo
    sorgu.exec("CREATE TABLE IF NOT EXISTS hayvanlar ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, ihbar_id INTEGER, tur TEXT, irk TEXT, cinsiyet TEXT, "
               "yas TEXT, cip TEXT, agirlik TEXT, renk TEXT, durum TEXT, kaynak TEXT, "
               "padok_no TEXT, medikal_notlar TEXT, "
               "dogum_tarihi TEXT, giris_tarihi TEXT, cikis_tarihi TEXT, resim_yolu TEXT)");
}

// --- MEDİKAL İŞLEM EKLEME BUTONU (YENİ) ---
void MainWindow::on_btnTedaviEkle_clicked()
{
    // Verileri al
    QString tarih = ui->dateTedaviIslem->date().toString("dd.MM.yyyy");
    QString tur = ui->cmbTedaviTur->currentText();
    QString notlar = ui->txtTedaviNot->text();

    if (notlar.isEmpty()) {
        QMessageBox::warning(this, "Eksik", "Lütfen yapılan işlemle ilgili kısa bir not giriniz.");
        return;
    }

    // HTML Formatında Şık Bir Satır Oluştur
    // Örn: 26.12.2025 | [AŞI] | Kuduz aşısı yapıldı
    QString yeniSatir = QString("<p><span style='color:#d9534f'><b>%1</b></span> &nbsp;|&nbsp; "
                                "<span style='color:#0275d8'><b>[%2]</b></span> &nbsp;|&nbsp; "
                                "%3</p>")
                            .arg(tarih)
                            .arg(tur)
                            .arg(notlar);

    // Mevcut notların altına ekle
    ui->txtMedikal->append(yeniSatir);

    // Giriş alanlarını temizle
    ui->txtTedaviNot->clear();
    ui->dateTedaviIslem->setDate(QDate::currentDate());
}

// --- HAYVAN KAYDET / GÜNCELLE ---
void MainWindow::on_btnHayvanKaydet_clicked()
{

    if (!ui->txtTedaviNot->text().isEmpty()) {
        on_btnTedaviEkle_clicked();
    }

    if (ui->cmbBagliIhbar->currentIndex() == -1) {
        QMessageBox::warning(this, "Eksik", "Lütfen yukarıdan bir İhbar Kaydı seçiniz.");
        return;
    }

    // Verileri Topla
    int ihbarID = ui->cmbBagliIhbar->currentData().toInt();
    QString tur = ui->cmbTur->currentText();
    QString irk = ui->txtIrk->text();
    QString cinsiyet = ui->cmbCinsiyet->currentText();
    QString yas = ui->txtYas->text();
    QString cip = ui->txtCip->text();
    QString agirlik = ui->txtAgirlik->text();
    QString renk = ui->txtRenk->text();
    QString durum = ui->cmbDurum->currentText();
    QString kaynak = ui->cmbKaynak->currentText();

    QString padok = ui->cmbPadok->currentText();
    // DİKKAT: Medikal notları HTML formatında alıyoruz
    QString medikal = ui->txtMedikal->toHtml();

    QString dogumTarihi = ui->dateDogum->date().toString("dd.MM.yyyy");
    QString girisTarihi = ui->dateGiris->date().toString("dd.MM.yyyy");
    QString cikisTarihi = "";
    if (ui->chkCikis->isChecked()) cikisTarihi = ui->dateCikis->date().toString("dd.MM.yyyy");

    QSqlQuery sorgu;
    if (duzenlenecekKayitID == -1) {
        sorgu.prepare("INSERT INTO hayvanlar (ihbar_id, tur, irk, cinsiyet, yas, cip, agirlik, renk, durum, kaynak, padok_no, medikal_notlar, dogum_tarihi, giris_tarihi, cikis_tarihi, resim_yolu) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    } else {
        sorgu.prepare("UPDATE hayvanlar SET ihbar_id=?, tur=?, irk=?, cinsiyet=?, yas=?, cip=?, agirlik=?, renk=?, durum=?, kaynak=?, padok_no=?, medikal_notlar=?, dogum_tarihi=?, giris_tarihi=?, cikis_tarihi=?, resim_yolu=? WHERE id = ?");
    }

    sorgu.addBindValue(ihbarID);
    sorgu.addBindValue(tur);
    sorgu.addBindValue(irk);
    sorgu.addBindValue(cinsiyet);
    sorgu.addBindValue(yas);
    sorgu.addBindValue(cip);
    sorgu.addBindValue(agirlik);
    sorgu.addBindValue(renk);
    sorgu.addBindValue(durum);
    sorgu.addBindValue(kaynak);
    sorgu.addBindValue(padok);
    sorgu.addBindValue(medikal);
    sorgu.addBindValue(dogumTarihi);
    sorgu.addBindValue(girisTarihi);
    sorgu.addBindValue(cikisTarihi);
    sorgu.addBindValue(secilenResimYolu);

    if (duzenlenecekKayitID != -1) sorgu.addBindValue(duzenlenecekKayitID);

    if (sorgu.exec()) {
        QMessageBox::information(this, "Başarılı", duzenlenecekKayitID == -1 ? "Yeni hayvan kaydedildi!" : "Güncelleme başarılı!");

        on_btnArsivListele_clicked(); // Tabloyu anında yenile
        istatistikleriGuncelle();

        // Temizlik
        ui->txtIrk->clear(); ui->txtCip->clear(); ui->txtYas->clear(); ui->txtAgirlik->clear(); ui->txtRenk->clear();
        ui->txtMedikal->clear(); ui->cmbPadok->setCurrentIndex(0);
        ui->lblResim->clear(); ui->lblResim->setText("Fotoğraf Yok");
        secilenResimYolu = "";
        ui->chkCikis->setChecked(false);
        duzenlenecekKayitID = -1;
        ui->btnHayvanKaydet->setText("Kaydet");
    } else {
        QMessageBox::critical(this, "Hata", "Kayıt hatası: " + sorgu.lastError().text());
    }
}

// --- AKILLI ÇİFT TIKLAMA ---
void MainWindow::on_tabloArsiv_cellDoubleClicked(int row, int column)
{
    QString id = ui->tabloArsiv->item(row, 0)->text();
    QMessageBox::StandardButton cevap = QMessageBox::question(this, "İşlem Seçimi", "Bu kaydı DÜZENLEMEK istiyor musunuz?\n(Hayır derseniz kimlik kartı açılır)", QMessageBox::Yes | QMessageBox::No);

    // KİMLİK KARTI GÖSTERİMİ
    if (cevap == QMessageBox::No) {
        QSqlQuery sorgu;
        sorgu.prepare("SELECT * FROM hayvanlar WHERE id = ?");
        sorgu.addBindValue(id.toInt());
        if (sorgu.exec() && sorgu.next()) {
            // Verileri HTML formatında hazırla
            QString mesaj = "<b>--- TEMEL BİLGİLER ---</b><br>"
                            "<b>Tür:</b> " + sorgu.value("tur").toString() + " / <b>Irk:</b> " + sorgu.value("irk").toString() + "<br>"
                                                                                                                  "<b>Cinsiyet:</b> " + sorgu.value("cinsiyet").toString() + " / <b>Yaş:</b> " + sorgu.value("yas").toString() + "<br>"
                                                                                                                       "<b>Çip No:</b> " + sorgu.value("cip").toString() + "<br>"
                                                              "<b>Padok:</b> " + sorgu.value("padok_no").toString() + "<br><br>"
                                                                   "<b>--- KLİNİK GEÇMİŞ ---</b><br>" + sorgu.value("medikal_notlar").toString(); // Zaten HTML kayıtlı

            QMessageBox msgBox;
            msgBox.setWindowTitle("Hayvan Kartı - ID: " + id);
            msgBox.setTextFormat(Qt::RichText);
            msgBox.setText(mesaj);
            msgBox.setStandardButtons(QMessageBox::Ok);

            QString resimYolu = sorgu.value("resim_yolu").toString();
            if(!resimYolu.isEmpty()) {
                QPixmap p(resimYolu);
                if(!p.isNull()) msgBox.setIconPixmap(p.scaled(150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            }
            msgBox.exec();
        }
        return;
    }

    // DÜZENLEME MODU
    QSqlQuery sorgu;
    sorgu.prepare("SELECT * FROM hayvanlar WHERE id = ?");
    sorgu.addBindValue(id.toInt());
    if (sorgu.exec() && sorgu.next()) {
        duzenlenecekKayitID = id.toInt();

        ui->cmbTur->setCurrentText(sorgu.value("tur").toString());
        ui->txtIrk->setText(sorgu.value("irk").toString());
        ui->cmbCinsiyet->setCurrentText(sorgu.value("cinsiyet").toString());
        ui->txtYas->setText(sorgu.value("yas").toString());

        // Çip Kontrol
        QString gelenCip = sorgu.value("cip").toString();
        if (gelenCip.isEmpty() || gelenCip == "Yok") {
            ui->chkCipVar->setChecked(false); ui->txtCip->setEnabled(false); ui->txtCip->clear();
        } else {
            ui->chkCipVar->setChecked(true); ui->txtCip->setEnabled(true); ui->txtCip->setText(gelenCip);
        }

        ui->txtAgirlik->setText(sorgu.value("agirlik").toString());
        ui->txtRenk->setText(sorgu.value("renk").toString());
        ui->cmbDurum->setCurrentText(sorgu.value("durum").toString());
        ui->cmbKaynak->setCurrentText(sorgu.value("kaynak").toString());
        ui->cmbPadok->setCurrentText(sorgu.value("padok_no").toString());

        // Medikal Notları HTML olarak yükle
        ui->txtMedikal->setHtml(sorgu.value("medikal_notlar").toString());

        ui->dateDogum->setDate(QDate::fromString(sorgu.value("dogum_tarihi").toString(), "dd.MM.yyyy"));
        ui->dateGiris->setDate(QDate::fromString(sorgu.value("giris_tarihi").toString(), "dd.MM.yyyy"));

        QString gelenCikis = sorgu.value("cikis_tarihi").toString();
        if (gelenCikis.isEmpty()) {
            ui->chkCikis->setChecked(false); ui->dateCikis->setEnabled(false);
        } else {
            ui->chkCikis->setChecked(true); ui->dateCikis->setEnabled(true); ui->dateCikis->setDate(QDate::fromString(gelenCikis, "dd.MM.yyyy"));
        }

        secilenResimYolu = sorgu.value("resim_yolu").toString();
        if(!secilenResimYolu.isEmpty()) {
            QPixmap p(secilenResimYolu);
            ui->lblResim->setPixmap(p.scaled(ui->lblResim->width(), ui->lblResim->height(), Qt::KeepAspectRatio));
        }

        ui->anaSekme->setCurrentIndex(2);
        ui->btnHayvanKaydet->setText("GÜNCELLE / KAYDET");
        QMessageBox::information(this, "Düzenleme", "Bilgiler yüklendi.");
    }
}

// --- TABLO LİSTELEME ---
void MainWindow::on_btnArsivListele_clicked()
{
    ui->tabloArsiv->setRowCount(0);
    ui->tabloArsiv->setColumnCount(11);
    ui->tabloArsiv->setHorizontalHeaderLabels({"ID", "Fotoğraf", "İhbar Kaydı", "Durum", "Padok", "Tür", "Irk", "Cinsiyet", "Çip No", "Giriş T.", "Çıkış T."});

    ui->tabloArsiv->verticalHeader()->setDefaultSectionSize(80);
    ui->tabloArsiv->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tabloArsiv->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->tabloArsiv->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    ui->tabloArsiv->setColumnWidth(1, 100);

    QString sorguMetni = "SELECT h.id, h.resim_yolu, i.isim, h.durum, h.padok_no, h.tur, h.irk, h.cinsiyet, h.cip, h.giris_tarihi, h.cikis_tarihi FROM hayvanlar h LEFT JOIN ihbarlar i ON h.ihbar_id = i.id WHERE 1=1 ";

    QString aranan = ui->txtArama->text();
    if(!aranan.isEmpty()) sorguMetni += " AND (i.isim LIKE '%" + aranan + "%' OR h.cip LIKE '%" + aranan + "%' OR h.irk LIKE '%" + aranan + "%' OR h.padok_no LIKE '%" + aranan + "%')";

    QString tur = ui->cmbArsivFiltre->currentText();
    if(tur != "Tümü" && !tur.isEmpty()) sorguMetni += " AND h.tur = '" + tur + "'";

    QSqlQuery sorgu;
    sorgu.exec(sorguMetni);

    while(sorgu.next()) {
        int satir = ui->tabloArsiv->rowCount();
        ui->tabloArsiv->insertRow(satir);
        ui->tabloArsiv->setItem(satir, 0, new QTableWidgetItem(sorgu.value(0).toString()));

        QString resimYolu = sorgu.value(1).toString();
        QTableWidgetItem *resimItem = new QTableWidgetItem();
        if(!resimYolu.isEmpty()) {
            QPixmap p(resimYolu);
            if(!p.isNull()) resimItem->setIcon(QIcon(p.scaled(100, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            else resimItem->setText("Yok");
        } else resimItem->setText("Yok");
        ui->tabloArsiv->setItem(satir, 1, resimItem);

        for(int i=2; i<=10; i++) ui->tabloArsiv->setItem(satir, i, new QTableWidgetItem(sorgu.value(i).toString()));
    }
}

// --- DİĞER YARDIMCI BUTONLAR (İhbar, Excel vb.) ---
void MainWindow::ihbarlariListele() {
    ui->tabloIhbarlar->setRowCount(0);
    QSqlQuery sorgu; sorgu.exec("SELECT * FROM ihbarlar");
    while (sorgu.next()) {
        int satir = ui->tabloIhbarlar->rowCount();
        ui->tabloIhbarlar->insertRow(satir);
        ui->tabloIhbarlar->setItem(satir, 0, new QTableWidgetItem(sorgu.value(0).toString()));
        ui->tabloIhbarlar->setItem(satir, 1, new QTableWidgetItem(sorgu.value(1).toString()));
        ui->tabloIhbarlar->setItem(satir, 2, new QTableWidgetItem(sorgu.value(2).toString()));
    }
}

void MainWindow::ihbarlariComboyaDoldur() {
    ui->cmbBagliIhbar->clear();
    QSqlQuery sorgu; sorgu.exec("SELECT id, isim FROM ihbarlar");
    while(sorgu.next()) ui->cmbBagliIhbar->addItem(sorgu.value(1).toString(), sorgu.value(0).toInt());
}

void MainWindow::on_btnIhbarKaydet_clicked() {
    QSqlQuery sorgu; sorgu.prepare("INSERT INTO ihbarlar (isim, adres) VALUES (?, ?)");
    sorgu.addBindValue(ui->txtIhbarSahibi->text()); sorgu.addBindValue(ui->txtIhbarAdres->toPlainText());
    if (sorgu.exec()) { ui->txtIhbarSahibi->clear(); ui->txtIhbarAdres->clear(); ihbarlariListele(); ihbarlariComboyaDoldur(); }
}

void MainWindow::on_btnFotoSec_clicked() {
    QString dosya = QFileDialog::getOpenFileName(this, "Fotoğraf Seç", "", "Resimler (*.png *.jpg *.jpeg)");
    if (!dosya.isEmpty()) { secilenResimYolu = dosya; QPixmap resim(dosya); ui->lblResim->setPixmap(resim.scaled(ui->lblResim->width(), ui->lblResim->height(), Qt::KeepAspectRatio)); }
}

// --- GÜNCELLENMİŞ EXCEL (CSV) AKTARMA BUTONU ---
void MainWindow::on_btnExcelAktar_clicked()
{
    QString dosyaAdi = QFileDialog::getSaveFileName(this, "Excel Olarak Kaydet", "Barinak_Listesi.csv", "Excel Dosyası (*.csv)");

    if (dosyaAdi.isEmpty()) return;

    QFile dosya(dosyaAdi);
    if (dosya.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream yaz(&dosya);
        // Türkçe karakter sorunu olmasın diye UTF-8 BOM ekliyoruz
        yaz.setGenerateByteOrderMark(true);
        yaz.setEncoding(QStringConverter::Utf16); // Excel genelde UTF-16 sever

        // 1. BAŞLIKLARI YAZ
        for (int i = 0; i < ui->tabloArsiv->columnCount(); i++) {
            yaz << ui->tabloArsiv->horizontalHeaderItem(i)->text() << "\t";
        }
        yaz << "\n";

        // 2. SATIRLARI YAZ
        for (int i = 0; i < ui->tabloArsiv->rowCount(); i++) {
            for (int j = 0; j < ui->tabloArsiv->columnCount(); j++) {

                if (ui->tabloArsiv->item(i, j)) {
                    QString veri = ui->tabloArsiv->item(i, j)->text();

                    // Temizlik (Satır atlamaları ve tabları boşluk yap)
                    veri.replace("\n", " ");
                    veri.replace("\t", " ");

                    // --- KRİTİK DÜZELTME BURADA ---
                    // Eğer sütun Çip Numarası (8) veya ID (0) ise
                    // Excel'in bilimsel sayı (1.23E+9) yapmasını engellemek için
                    // Başına tek tırnak (') koyuyoruz. Excel bunu "Metin" olarak algılar.
                    if (j == 8 || j == 0) {
                        veri = "'" + veri;
                    }
                    // ------------------------------

                    yaz << veri << "\t";
                } else {
                    yaz << "\t";
                }
            }
            yaz << "\n";
        }

        dosya.close();
        QMessageBox::information(this, "Başarılı", "Liste Excel uyumlu olarak kaydedildi!\n\nNot: Excel'de #### gördüğünüz sütunların genişliğini fareyle açarsanız tarihleri görebilirsiniz.");
    } else {
        QMessageBox::critical(this, "Hata", "Dosya kaydedilemedi!");
    }
}

void MainWindow::on_btnIhbarSil_clicked() {
    int satir = ui->tabloIhbarlar->currentRow();
    if(satir>=0 && QMessageBox::question(this,"Sil","Emin misin?",QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes) {
        QSqlQuery s; s.prepare("DELETE FROM ihbarlar WHERE id=?"); s.addBindValue(ui->tabloIhbarlar->item(satir,0)->text().toInt());
        s.exec(); ihbarlariListele(); ihbarlariComboyaDoldur();
    }
}

void MainWindow::on_btnArsivSil_clicked() {
    int satir = ui->tabloArsiv->currentRow();
    if(satir>=0 && QMessageBox::question(this,"Sil","Kayıt silinsin mi?",QMessageBox::Yes|QMessageBox::No)==QMessageBox::Yes) {
        QSqlQuery s; s.prepare("DELETE FROM hayvanlar WHERE id=?"); s.addBindValue(ui->tabloArsiv->item(satir,0)->text().toInt());
        if(s.exec()) on_btnArsivListele_clicked();
    }
}

void MainWindow::on_chkCipVar_toggled(bool checked) { ui->txtCip->setEnabled(checked); if(!checked) ui->txtCip->clear(); }
void MainWindow::on_chkCikis_toggled(bool checked) { ui->dateCikis->setEnabled(checked); if(checked) ui->dateCikis->setDate(QDate::currentDate()); }

// --- DASHBOARD (KOMUTA MERKEZİ) OLUŞTURMA ---
void MainWindow::dashboardOlustur() {
    // 1. Yeni Bir Sekme Yarat
    tabDashboard = new QWidget();

    // Grid Düzeni (Kutuları 2x2 dizeceğiz)
    QGridLayout *layout = new QGridLayout(tabDashboard);
    layout->setSpacing(20); // Kutular arası boşluk
    layout->setContentsMargins(30, 30, 30, 30); // Kenar boşlukları

    // Kutu Tasarım Stilleri (CSS Benzeri)
    QString stilMavi = "QFrame { background-color: #3498db; border-radius: 10px; } QLabel { color: white; border: none; }";
    QString stilYesil = "QFrame { background-color: #27ae60; border-radius: 10px; } QLabel { color: white; border: none; }";
    QString stilTuruncu = "QFrame { background-color: #e67e22; border-radius: 10px; } QLabel { color: white; border: none; }";
    QString stilKirmizi = "QFrame { background-color: #e74c3c; border-radius: 10px; } QLabel { color: white; border: none; }";

    // --- KUTU 1: TOPLAM HAYVAN ---
    QFrame *fr1 = new QFrame(); fr1->setStyleSheet(stilMavi);
    QVBoxLayout *l1 = new QVBoxLayout(fr1);
    QLabel *lblBaslik1 = new QLabel("TOPLAM HAYVAN"); lblBaslik1->setFont(QFont("Arial", 12, QFont::Bold));
    lblToplamHayvan = new QLabel("0"); lblToplamHayvan->setFont(QFont("Arial", 36, QFont::Bold)); lblToplamHayvan->setAlignment(Qt::AlignCenter);
    l1->addWidget(lblBaslik1, 0, Qt::AlignCenter); l1->addWidget(lblToplamHayvan);
    layout->addWidget(fr1, 0, 0);

    // --- KUTU 2: BU AY SAHİPLENİLEN ---
    QFrame *fr2 = new QFrame(); fr2->setStyleSheet(stilYesil);
    QVBoxLayout *l2 = new QVBoxLayout(fr2);
    QLabel *lblBaslik2 = new QLabel("BU AY SAHİPLENİLEN"); lblBaslik2->setFont(QFont("Arial", 12, QFont::Bold));
    lblSahiplendirilen = new QLabel("0"); lblSahiplendirilen->setFont(QFont("Arial", 36, QFont::Bold)); lblSahiplendirilen->setAlignment(Qt::AlignCenter);
    l2->addWidget(lblBaslik2, 0, Qt::AlignCenter); l2->addWidget(lblSahiplendirilen);
    layout->addWidget(fr2, 0, 1);

    // --- KUTU 3: TEDAVİ GÖRENLER ---
    QFrame *fr3 = new QFrame(); fr3->setStyleSheet(stilTuruncu);
    QVBoxLayout *l3 = new QVBoxLayout(fr3);
    QLabel *lblBaslik3 = new QLabel("TEDAVİ GÖRENLER"); lblBaslik3->setFont(QFont("Arial", 12, QFont::Bold));
    lblTedaviGoren = new QLabel("0"); lblTedaviGoren->setFont(QFont("Arial", 36, QFont::Bold)); lblTedaviGoren->setAlignment(Qt::AlignCenter);
    l3->addWidget(lblBaslik3, 0, Qt::AlignCenter); l3->addWidget(lblTedaviGoren);
    layout->addWidget(fr3, 1, 0);

    // --- KUTU 4: BUGÜN GELENLER ---
    QFrame *fr4 = new QFrame(); fr4->setStyleSheet(stilKirmizi);
    QVBoxLayout *l4 = new QVBoxLayout(fr4);
    QLabel *lblBaslik4 = new QLabel("BUGÜN GELENLER"); lblBaslik4->setFont(QFont("Arial", 12, QFont::Bold));
    lblBugunGelen = new QLabel("0"); lblBugunGelen->setFont(QFont("Arial", 36, QFont::Bold)); lblBugunGelen->setAlignment(Qt::AlignCenter);
    l4->addWidget(lblBaslik4, 0, Qt::AlignCenter); l4->addWidget(lblBugunGelen);
    layout->addWidget(fr4, 1, 1);

    // Sekmeyi En Başa Ekle

    ui->anaSekme->insertTab(0, tabDashboard, "GENEL DURUM (DASHBOARD)");
    ui->anaSekme->setCurrentIndex(0);
}

// --- VERİTABANINDAN SAYILARI ÇEKME ---
void MainWindow::istatistikleriGuncelle() {
    QSqlQuery sorgu;

    // 1. Toplam Hayvan (Barınaktaki aktif hayvanlar)
    sorgu.exec("SELECT count(*) FROM hayvanlar");
    if(sorgu.next()) lblToplamHayvan->setText(sorgu.value(0).toString());

    // 2. Bu Ay Sahiplendirilenler
    // Tarih formatımız: dd.MM.yyyy -> Sadece Ay ve Yıl tutan kısmı arıyoruz (%.MM.yyyy)
    QString buAy = QDate::currentDate().toString(".MM.yyyy");
    sorgu.prepare("SELECT count(*) FROM hayvanlar WHERE durum='Sahiplendirildi' AND cikis_tarihi LIKE ?");
    sorgu.addBindValue("%" + buAy);
    if(sorgu.exec() && sorgu.next()) lblSahiplendirilen->setText(sorgu.value(0).toString());

    // 3. Tedavi Görenler
    sorgu.exec("SELECT count(*) FROM hayvanlar WHERE durum='Tedavi Altında' OR durum='Karantinada' OR durum='Rehabilitasyon Aşamasında'");
    if(sorgu.next()) lblTedaviGoren->setText(sorgu.value(0).toString());

    // 4. Bugün Gelenler
    QString bugun = QDate::currentDate().toString("dd.MM.yyyy");
    sorgu.prepare("SELECT count(*) FROM hayvanlar WHERE giris_tarihi = ?");
    sorgu.addBindValue(bugun);
    if(sorgu.exec() && sorgu.next()) lblBugunGelen->setText(sorgu.value(0).toString());
}


void MainWindow::veritabaniYedekle() {
    // 1. Yedekler Klasörünü Kontrol Et (Yoksa Oluştur)
    QDir dir;
    if (!dir.exists("Yedekler")) {
        dir.mkdir("Yedekler");
    }

    // 2. Orijinal Dosya ve Yeni İsim
    QString orjinalDosya = "barinak_v3.db"; // Kullandığımız dosya

    // Tarih ve Saat damgası oluştur (Örn: 26.12.2025_17-30-05)
    QString zamanDamgasi = QDate::currentDate().toString("dd.MM.yyyy") + "_" +
                           QTime::currentTime().toString("HH-mm-ss");

    QString yedekDosya = "Yedekler/barinak_Yedek_" + zamanDamgasi + ".db";

    // 3. Kopyalama İşlemi
    // Veritabanı o an açık olsa bile QFile::copy genelde okuma modunda kopyalayabilir.
    // Ancak en garantisi, veritabanı bağlantısını kapatıp kopyalamaktır ama
    // SQLite tek dosya olduğu için canlı kopyalama da çoğu zaman çalışır.

    if (QFile::copy(orjinalDosya, yedekDosya)) {
        // qDebug() << "Yedek alındı:" << yedekDosya; // İstersen loga yaz
    }
}

// --- OTOMATİK DURUM KONTROLÜ ---
void MainWindow::on_cmbDurum_currentTextChanged(const QString &arg1)
{
    // Eğer kullanıcı durumu "Sahiplendirildi" veya "Ex (Vefat)" yaparsa
    // Otomatik olarak çıkış tarihini aç ve bugünü seç.
    if (arg1 == "Sahiplendirildi" || arg1 == "Ex" || arg1 == "Transfer") {
        ui->chkCikis->setChecked(true); // Kutuyu işaretle
        // Tarih zaten chkCikis tetiklenince bugüne dönüyor, ekstra koda gerek yok.
    }
    // Eğer tekrar "Barınakta" veya "Tedavi Altında" yaparsa çıkışı kapat
    else if (arg1 == "Barınakta" || arg1 == "Tedavi Altında" || arg1 == "Karantinada") {
        ui->chkCikis->setChecked(false); // Kutuyu kaldır
    }
}
