# isletim-sistemleri-proje-odevi-56.grup
    HAZIRLAYANLAR
    ----------------
    Can Örge    G221210028  2-A
    Ömer Keleş  B221210022  1-A
    Hasan Bera Aydın B221210010 1-A
    Yusuf Kaçmaz    G221210007  2-A    
    Alper Bora      B221210307  1-A  


    İSTENİLENLER
   ------------------

    1- PROMPT 
    -------------
    printf(">");
    fflush(stdout);

            İstenilen şekilde çalışmaktadır

    2- Quit 
    -------------
    >quit
            İstenilen şekilde çalışmaktadır

    3- Tekli Komutlarü
    -------------
    >ls -1
            İstenilen şekilde çalışmaktadır
    
    4- Giriş Yönlendirme
    -------------
    > cat < file.txt
    file.txt'nin içinde ne varsa ekrana yazdırır.
    > cat < nofile.txt
    nofile.txt giriş dosyası bulunamadı
            İstenilen şekilde çalışmaktadır
    
    5- Çıkış Yönlendirme
    -------------
    > cat > file2
            İstenilen şekilde çalışmaktadır
    
    6- Arkaplan Çalışma
    -------------
    > sleep 5 &
    > cat file.txt
    file.txt’nin içindekiler ekrana yazdırılır.
    > [24617] retval: 0
            İstenilen şekilde çalışmaktadır
    
    7- Boru(pipe)
    -------------
    > find /etc | grep ssh | grep conf
            İstenilen şekilde çalışmaktadır
