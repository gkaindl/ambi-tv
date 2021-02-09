# ambi-tv

Eine Weiterentwicklung von gkaindl's Framework für ein Ambilight mit einem Embedded-Linux-System (z.B. Raspberry), einem USB-Framegrabber und adressierbaren RGB-LED-Stripes

Hier ist eine [Video-Demonstration](https://www.youtube.com/watch?v=gjBJl8lVzbc) des Standard-Modus. Ein Beispiel für die zusätzliche Audio-Funktion findet Ihr [hier](https://youtu.be/Ifz-ns2uNhg).

ambi-tv basiert auf der Idee, einen HDMI-Splitter und einen HDMI-zu-CVBS-Umsetzer für das parallele Erfassen des angezeigten Bildes und dessen Darstellung auf einem LED-Stripe zu verwenden. Das funktioniert mit jeder HDMI-Quelle ohne die Notwendigkeit, einen zusätzlichen Computer zu verwenden. 

Die Software ist für eine möglichst einfache Erweiterung und Anpassung ausgelegt.

Mit einem lauffähig zu kaufenden Raspberry Pi ist es sehr leicht, ein eigenständiges Ambilight aufzubauen.


## Unterschiede zum Original-ambi-tv

- durch Verwendung eines Kernels >= 3.14 muß der Treiber für den Video-Grabber nicht extra gebaut werden sondern ist schon vorhanden
- es kann zwischen den Video-Normen PAL und NTSC gewählt werden
- der nun ebenfalls verfügbare Audiograbber-Treiber ermöglicht die tonabhängige Steuerung der LED (Spektrum-Analyzer)
- Programme und Parameter können über Netzwerk kontrolliert werden, der Taster ist im Prinzip überflüssig
- die 3D-Modi Side-by-Side (SBS) und Top-over-Bottom (TOB) können für eine richtig skalierte LED-Ansteuerung aktiviert werden
- neben dem Chiptyp LPD8806 auf dem LED-Stripe werden nun auch die Chiptypen WS280x, WS281x, SK6812 und APA10x unterstützt
- die Randerkennung wurde überarbeitet um nicht durch Senderlogos oder Artefakte an den Seitenrändern gestört zu werden
- es kann eine Minimalhelligkeit der LED festgelegt werden, um das Ambilight auch als einzige Raumbeleuchtung verwenden zu können

## Verwendete GIT-Projekte

Neben dem Grundgerüst von Georg Kaindl wurde auch auf die Vorabeiten von [Karl Stavestrand](https://github.com/karlstav/cava) (Spektrum-Analyzer mit fftw3 und ALSA-Quelle) und [Jeremy Garff](https://github.com/jgarff/rpi_ws281x) (Ansteuerung der WS281x-Chips über PWM und DMA) zurückgegriffen.  

## Hardware Setup

Für den Aufbau des eigenständigen Ambilights werden folgende Komponenten benötigt:

- Raspberry Pi (beliebige Version mit aktuellem [Raspbian-Image](http://downloads.raspberrypi.org/raspbian_latest)) mit [Kühlkörpern](http://www.amazon.de/gp/product/B00BB8ZB4U)
- [HDMI-Splitter](http://www.amazon.de/gp/product/B0081FDFMQ): Dieser Splitter unterstützt auch BD-3D und CEC.
- [HDMI / Composite Umsetzer](http://www.amazon.de/gp/product/B00AASZU8E): erzeugt auch das Audio-Signal für den Spektrum-Analyzer.
- [LPD8806 RGB LED Strip](http://www.watterott.com/de/Digital-Addressable-RGB-LED): Die benötigte Länge hängt von der Bildschirmdiagonale ab.
- alternativ [WS2811 RGB LED Strip](http://www.ebay.de/itm/WS2812B-LED-Stripe-4m-RGB-60-LEDs-m-Klebestreifen-WS2811-WS2812-/251901768682?pt=LH_DefaultDomain_77&hash=item3aa683f3ea): Vorteile: höhere LED-Dichte, günstigerer Preis, kein Chip sichtbar (nur die LED sind zu sehen) Nachteil: man benötigt einen Pegelwandler von den 3,3V des Raspi auf die 5V der LED. 
- [USB-Video-Grabber](https://www.amazon.de/gp/product/B0013BXFLG): Muß einen Fushicai-Chipsatz besitzen, damit der usbtv-Treiber funktioniert.
- [5V-Netzteile](http://www.amazon.de/gp/product/B004S7U4IO): Für eine höhere LED-Zahl sollten es mindestens 3 Stück sein. Eins für den Raspberry und das Zubehör (die können über einen solchen [Hub](http://www.amazon.de/gp/product/B00UBROY0Y) mit dem Netzteil verbunden werden) und zwei für die LED-Stripes (an beiden Enden einspeisen)
- optional [PCM/Analog-Wandler](http://www.amazon.de/gp/product/B004B662AG): um den Digital-Ausgangs eines DTS-Decoders als Audioquelle nutzen zu können
- optional [Dreikantleisten](http://www.cmc-versand.de/Aeronaut/Balsa-Dreikant-15x15-mm-arnr-48-754315.html): mit ihrer Hilfe strahlen die LED an einer senkrechten Fernseher-Rückwand nich direkt nach hinten an die Wand sondern in einem Winkel von 45° nach außen. Dadurch geht nicht so viel Licht verloren. Die Befestigung kann mit solchem dünnen [Klebeband](http://www.dx.com/p/m001-8mm-x-50m-double-sided-tape-black-258686) erfolgen.
- Tipp-Taster: Für diejenigen, die die Umschaltung nicht über das Web-Interface vornehmen.
- diverse Kabel, Lötwerkzeug

Hardwareaufbau:

- Der Aufbau eines solchen Systems ist z.B. [hier](http://www.forum-raspberrypi.de/Thread-tutorial-ambi-tv-ambilight-fuer-hdmi-quellen) gut beschrieben. Die Punkte 3 und 4 müssen übersprungen werden und es sollte das aktuellste Raspbian-Image verwendet werden.

- Zur Übersicht noch einmal die Auflistung der vorhandenen Signale:

	Raspberry Pi		Signal
	---------------------------------
	P1/19 (MOSI)		LPD880x DATA
	P1/23 (SCLK)		LPD880x CLOCK
	P1/12 (PWM)			WS28xx DATA
	P1/5				Taster Pin A
	P1/6				Taster Pin B
    
Hier die Pinbelegung des Raspberry:

![Raspberry Pi Wiring](doc/rpi-wiring.jpg)

Wie oben schon angemerkt, muß bei Verwendung von WS281x-LED das 3,3V-Ausgangssignal des Raspberry mit einem Transistor oder einem Pegelwandlerschaltkreis auf 5V angehoben werden. Ob man den Wandler invertierend oder nichtinvertierend aufbaut ist unerheblich. Man muß die mögliche Invertierung aber später in der Konfigurationsdatei berücksichtigen.

Das HDMI-Signal wird von der zentralen Quelle an den HDMI-Splitter geführt und von dort auf den Fernseher und unseren HDMI/Composite-Umsetzer aufgeteilt. Audio- und Videoausgänge des Umsetzers werden mit den zugehörigen Eingängen des USB-Grabbers verbunden, welcher im Raspberry steckt.  
Leider kann der Umsetzer nur Stereo-PCM-Signale in ein für den Audio-Grabber verständliches Signal wandeln. Kommt über HDMI ein DTS-Digitalsignal, kann der Grabber mit dem Audiosignal des Umsetzers nichts anfangen. Wer also DTS-Sound nutzen möchte, sollte den Digitalausgang des Fernsehers auf PCM-Modus einstellen, dieses Signal mit dem oben aufgelisteten PCM/Analog-Wandler in ein für den Grabber nutzbares Signal umwandeln und dieses Signal statt des Umsetzer-Ausgangs verwenden. Der Fernseher übernimmt dabei die DTS-Decodierung und stellt gleichzeitig ein Signal mit konstanter Amplitude zur Verfügung.

## Software Installation

Bevor ambi-tv verwendet werden kann, werden für den Audio-Spektrum-Analyzer noch einige Bibliotheken und Tools benötigt. Diese kann man sich durch Eingabe von `'sudo apt-get install git libfftw3-dev libasound2-dev alsa-utils'` installieren.  
Um zu prüfen, ob nach dem Anstecken des USB-Grabbers alle Treiber geladen worden sind, schaut man mit `'ls /dev | grep video'` ob das Gerät "video0" vorhanden ist. Ob der Audiograbber-Treiber geladen wurde, sieht man mit `'arecord -l'`. Hier sollte der usbtv-Treiber angezeigt werden:

    Liste der Hardware-Geräte (CAPTURE)
    Karte 0: usbtv [usbtv], Gerät 0: USBTV Audio [USBTV Audio Input]
    	Sub-Geräte: 1/1
    	Sub-Gerät #0: subdevice #0
   
Die Kartenummer "0" und Subgerätenummer "0" merken wir uns.

Wird ein SPI-LED-Streifen verwendet, muß sichergestellt werden, daß der SPI-Treiber geladen wird. Das läßt sich am einfachsten über "raspi-config" einstellen.

Nun clonen wir das ambi-tv-Repository mit `'git clone http://github.com/xSnowHeadx/ambi-tv.git ambi-tv'` in das Nutzerverzeichnis (in der Regel "pi"). Mit `'cd ambi-tv'` wechseln wir in das ambi-tv-Verzeichnis und bauen das Projekt mit `'make'`. Die ausführbare Datei finden wir nun im Verzeichnis "bin".
Zum Installieren mit Autostart führt man `'sudo make install'` aus. Nun wird ambi-tv bei jedem Start des Raspberry automatisch mit gestartet. 

Folgende Parameter akzeptiert ambi-tv beim Start:

- `-b/--button-gpio [i]`: Gibt den GPIO-Pin an, an welchem der Taster angeschlossen ist. Geschah das wie oben beschrieben, wäre das die `3` bei einem Raspberry Rev. B  (die `1` bei Rev. A). Standardmäßig wird `-1` verwendet, was bedeutet, daß der Taster ignoriert wird.
- `-f/--file [path]`: Der Pfad zum Konfigurationsfile, welches verwendet werden soll. Ohne Angabe wird `/etc/ambi-tv.conf` verwendet.
- `-h,--help`: Gibt einen Hilfebildschirm aus und beendet dann das Programm wieder.
- `-p,--program [i]`: Programmnummer, welche ambi-tv beim Start aktivieren soll. Standardmäßig ist das die `0`, also das erste Programm.
- `-s,--socketport [i]`: Port, auf welchem das Programm auf eingehende Kommunikation auf dem Web-Interface lauscht. Standardmäßig ist das der Port 16384.

Wurde ambi-tv von der Konsole aus gestartet, kann mit der Leertaste zwischen den einzelnen Programmen in der Reihenfolge durchgeschaltet werden, in welcher sie im Konfigurationsfile definiert wurden.  
Mit der `t`-Taste kann man zwischen Pause und Programmlauf hin- und herschalten. Im Pause-Modus sind alle Komponenten deaktiviert.

Mit dem über die Kommandozeile aktiviertem Taster kann durch einmaliges Klicken zwischen Pause und Programmlauf gewechselt werden. Ein "Doppelklick" schaltet ähnlich der Leertaste zwischen den definierten Programmen um. 

Im Repository liegt eine Beispiel-Konfigurationsdatei mit einer Zusammenstellung möglicher Programme. Die Parameter der Konfigurationsdatei werden im Folgenden erläutert.

## Konfigurationsdatei

ambi-tv verwendet eine Konfigurationsdatei für die Definition und Parametrierung von Komponenten und Programmen. Es können sogenannte **components** (also Funktionen) erstellt und parametriert sowie **programs** (also Programme), welche eine Zusammenstellung von Eingangs-, Verarbeitungs- und Ausgabefunktionen darstellen, festgelegt werden.

Eine Komponente ist ein Teil des Datenflusses in ambi-tv. Es gibt Quellen (Video-Grabber, Audio-Grabber oder einfache Binärdatengeneratoren), Prozessoren (Verarbeitungskomponenten, welche die Quelldaten auswerten) und Senken (geben die bearbeiteten Daten an den LED-String weiter). Die gleiche Komponente kann beliebig oft bei feststehendem Komponenten-Namen unter verschiedenen Instanzen-Namen und mit jeweils unterschiedlichen Parametern angelegt werden. Eine Komponentendefinition (zwei Instanzen der gleichen Komponente mit unterschiedlichen Parametern) sieht so aus:

    component_name {
        name            instance_nameA
        settingA        valueA
        settingB        valueB
    }

    component_name {
        name            instance_nameB
        settingA        valueC
        settingB        valueD
    }

Ein Programm besteht aus einer Kombination einzelner Komponenteninstanzen (im Normalfall eine Quelle, ein Prozessor und eine Senke). Es können beliebig viele Programme erstellt werden. Eine Programmdefinition sieht so aus (das "&" Zeichen am Anfang des Programmnamens ist zwingend notwendig):

    &program_name {
        activate        instance_name_source
        activate        instance_name_processor
        activate        instance_name_sink
        ...
    }

Am Einfachsten sieht man die Vorgehensweise in der Beispiel-Konfigurationsdatei.

## Verfügbare Komponenten

Im Moment unterstützt ambi-tv folgende Komponententypen mit ihren Einstellungen:

**v4l2-grab-source**: Der Video-Grabber mit video4linux -> mmap()-basierender Erfassung.  

- `name`: Der Instanzenname der Quelle, unter welchem sie mit den eingestellten Parametern in den Programmen verwendet werden kann.  
- `video-device`: Der Name des geladenen Video-Devices. In der Regel ist das `/dev/video0`.  
- `video-norm`: Die genutzte Video-Norm. Es kann zwischen `PAL` (720x576 Pixel) und `NTSC` (720x480 Pixel) gewählt werden.  
- `buffers`: Die vom Kernel für die Bilddaten angeforderten Puffer. Ein Wert zwischen 4 und 8 ist sinnvoll.  
- `crop-top`, `crop-bottom`, `crop-left`, `crop-right`: Die Anzahl der Pixel, welche an den jeweiligen Rändern übersprungen und nicht ausgewertet werden sollen. Das ist zur Unterdrückung von Bildstörungen und Wandlungsartefakten sinnvoll.  
- `autocrop-luminance-threshold`: Ein Wert zwischen 0 und 255, welcher als Schwellwert beim Überspringen von schwarzen Balken an den Bildrändern verwendet wird. Wird dieser Helligkeitswert innerhalb einer Zeile oder Spalte 5 Mal überschritten, wird das als Ende des schwarzen Balkens interpretiert und die folgenden Zeilen bzw. Spalten an die Auswertung weitergegeben. Zu beachten ist, daß das Finden der schwarzen Balken und die oben eingestellten Bildschirmränder zusammenwirken. Eine negative Zahl schaltet das Suchen des schwarzen Balkens aus (default).

**audio-grab-source**: Der Audio-Grabber über das ALSA-Device des USB-Grabbers.  

- `name`: Der Instanzenname der Quelle, unter welchem sie mit den eingestellten Parametern in den Programmen verwendet werden kann.  
- `audio-device`: Der Name des geladenen Audio-Grabber-ALSA-Devices. Die Geräte- und Subgeräte-Nummer hatten wir uns ja oben gemerkt. Der Devicename lautet im Beispiel also `hw:0,0`.  

**timer-source**: Diese Quelle schiebt nur zyklisch eine Prozessorkomponente an ohne selbst Daten zu liefern. Diese müssen in der Prozessorkomponente generiert werden (Beispiel Mood-Light)

- `name`: Der Instanzenname der Quelle, unter welchem sie mit den eingestellten Parametern in den Programmen verwendet werden kann.  
- `millis`: Die Zeit in Millisekunden zwischen dem zyklischen Starts der Prozessorkomponente.

**avg-color-processor**: Berechnet den Farbmittelwert des gesamten Bildes und stellt alle LED auf diese Farbe ein. Das kann verwendet werden wenn die unmittelbare Übertragung der Randfarben auf die LED beim edge-color-processor zu unruhig sein sollt. Außer dem Namen hat diese Komponente keine weiteren Parameter.

- `name`: Der Instanzenname des Prozessors, unter welchem er mit den eingestellten Parametern in den Programmen verwendet werden kann.  

**edge-color-processor**: Bildet die Farben der Bildränder auf die LED ab. Das ist der klassische Ambilight-Effekt.

- `name`: Der Instanzenname des Prozessors, unter welchem er mit den eingestellten Parametern in den Programmen verwendet werden kann.  
- `vtype`: Dient der Anpassung an die Übertragung von 3D-Bildern im Side-by-Side- oder Top-over-Bottom-Format. Ohne diese Anpassung wäre sonst die Darstellung der Randfarben verzerrt.
  * `0` bedeutet "kein 3D". Das Bild wird normal ausgewertet
  * `1` bedeutet "3D im SBS-Format". Es wird nur die linke Bildhälfte ausgewertet und auf die gesamte Bildbreite skaliert.
  * `2` bedeutet "3D im TOB-Format". Es wird nur die obere Bildhälfte ausgewertet und auf die gesamte Bildhöhe skaliert.  
- `box-width`, `box-height`: Die Breite und Höhe des Rechtecks in Pixeln, welches um eine LED herum für die Farbermittlung herangezogen wird. Die LED wird auf den Farbmittelwert dieses Rechtecks gesetzt. Je kleiner das Rechteck um so detaillierter aber auch unruhiger wird der Effekt. Hier sollte man etwas experimentieren, um den an Bildschirmbreite und LED-Anzahl angepaßten optimalen Effekt zu bekommen.

**mood-light-processor**: Erzeugt auch ohne Eingangsdaten ein Mood-Light, indem der komplette HSL-Farbraum dargestellt und langsam durchgeschoben wird.

- `name`: Der Instanzenname des Prozessors, unter welchem er mit den eingestellten Parametern in den Programmen verwendet werden kann.  
- `speed`: Schrittweite, mit welcher der Farbraum durchgeschoben wird. In Verbindung mit dem "millis"-Parameter der Timerquelle ergibt sich so die Geschwindigkeit der Farbänderung.
- `mode`: Legt die Art der Farbdarstellung fest
  * `0` bedeutet es wird ein diagonal über die Ecken verlaufendes Farbband angezeigt
  * `1` bedeutet es wird ein den ganzen Bildschirm umlaufendes Farbband angezeigt
  * `2` bedeutet der ganze LED-Streifen wird in einer Farbe dargestellt, welche langsam das gesamte Spektrum durchläuft

**audio-processor**: Verarbeitet die erfaßten Audio-Daten mittels FFT und wandelt sie in Farben um.

- `name`: Der Instanzenname des Prozessors, unter welchem er mit den eingestellten Parametern in den Programmen verwendet werden kann.  
- `atype`: Legt die Art und Weise der Verarbeitung der Audiodaten fest.
  * `0` bedeutet "Audio-Spektrum". Das FFT-Ergebnis wird als Farbband um den Bildschirm herum abgebildet. Die tiefen Frequenzen (rot) liegen dabei unten in der Mitte und laufen über das gesamte Farbband um den Bildschirm herum bis oben zu den höchsten Frequenzen (weiß)
  * `1` bedeutet "Audio-Mittelwert". Auch hier wird das Spektrum zunächst auf das Farbspektrum abgebildet. Es wird aber kein Farbband ausgegeben sondern der sich aus allen berechneten Farben ergebende Mittelwert wird auf allen LED gleich ausgegeben. Das ergibt einen nicht so unruhigen Effekt wie das Spektrum.
  * `2` bedeutet "Level-Meter". Es leuchtet ein LED-Band von der unteren zur oberen Mitte des Bildschirms. Dessen Länge ist vom Pegel abhängig. Die Farbe wird mit dem Parameter "levelcolor" festgelegt (s.u.) 
- `levelcolor`: Bestimmt die Farbe des Leuchtbalkens im Modus 2 im hexadezimalen Format RRGGBB (FF0000 ist rot, 00FF00 grün, FFFF00 gelb usw.). Bei einem Wert von 000000 wird statt der vorgegebenen Farbe der Farbwert, welcher auch beim Average-Modus berechnet wurde, verwendet. Dieser wird allerdings auf 100% Helligkeit normiert, da sich hier die Helligkeit aus der Länge des Leuchtbalkens ergibt. In den Modi 0 und 1 wird dieser Wert zwar nicht verwendet, muß aber angegeben werden (kann 000000 sein).
- `sensitivity`: Legt die prozentuale Verstärkung des Audiosignals vor der Verarbeitung fest. Damit kann das Ergebnis an kleinere oder stärkere Eingangspegel angepaßt werden. Werte zwischen 0 und 1000 sind möglich. 100 entspricht 1:1.
- `smoothing`: Sorgt für eine Glättung des FFT-Ergebnisses um den optischen Effekt zu beruhigen und ein Flackern zu vermeiden. Es stehen drei Glättungsfilter zur Auswahl:
  * `1` bedeutet "Falloff-Filter", welches das Fallen des zugehörigen Pegels einer Gravitationssimulation entsprechend verzögert. Zunächst fällt der Pegel ohne weiteres Signal langsam, dann immer schneller. 
  * `2` bedeutet "Mittelwert-Filter". Dieses mittelt vergangene und aktuelle Pegel und sorgt so für einen verzögerten Pegelabfall.
  * `4` bedeutet "Integrator-Filter". Das addiert die Pegel der Vergangenheit auf und fällt nur langsam ab. Dieses Filter sollte immer aktiviert sein, um die volle LED-Helligkeit zu erreichen.  
Es können mehrere Filter gleichzeitig aktiviert werden indem deren Zahlen addiert werden. `5` würde also z.B. Falloff- und Integrator-Filter gleichzeitig aktivieren. `0` deaktiviert die Glättung komplett. 
- `linear`: Das Ergebnis der FFT ist logarithmisch. Kleinere Ausschläge bei einer bestimmten Frequenz werden also verstärkt dargestellt. Das kann zum optischen Verschwinden der Unterschiede zwischen den einzelnen Frequenzanteilen führen. Mit `1` kann deshalb die Linearisierung aktiviert werden, welche die Anhebung der geringeren Pegel rückgängig macht und für eine bessere Kanaltrennung sorgt. `0` schaltet die Linearisierung aus.

**ledstripe-sink**: Die eigentliche Ansteuerung der LED Stripes.

- `name`: Der Instanzenname der Senke, unter welchem sie mit den eingestellten Parametern in den Programmen verwendet werden kann.  
- `led-device`: Das verwendete Device. Für LPD880x, APA10x bzw. WS280x z.B. `/dev/spidev0.0`. Für einen SK6812- oder WS281x-Stripe z.B. `DMA5` für den DMA-Kanal 5
- `dev-speed-hz`: Die Taktfrequenz für die Datenausgabe. Für LDP880x, APA10x  bzw. Ws280x z.B. `2500000` (2.5MHz). Für einen SK6812- bzw. WS281x-Stripe sind entweder `400000` oder `800000` möglich, abhängig von der Beschaltung der Chips durch den Stripe-Hersteller .
- `dev-type`: Der angeschlossene LED-Stripe. Gültige Werte sind `LPD880x`, `WS280x`, `APA10x`, `WS281x` und `SK6812`
- `dev-pin`: Nur für WS281x und SK6812. Der GPIO-Pin, an welchem die Daten ausgegeben werden sollen. Standard ist `18`.
- `dev-inverse`: Nur für WS281x und SK6812. Gibt an, ob der Pegelwandler das Ausgangssignal invertiert (`1`) oder nicht (`0`).
- `dev-color-order`: Gibt an, in welcher Reihenfolge  die Bytes der einzelnen Farben an die LED gesendet werden müssen. "RGB" bedeutet, daß zunächst das Byte für Rot, dann für Grün und zuletzt für Blau gesendet wird. Dieser Parameter ist optional und wird normalerweise automatisch passend zum ausgewählten LED-Typ gesetzt.
- `leds-top`, `leds-left`, `leds-bottom`, `leds-right`: Beschreibt die LED-Positionen auf den Bildschirmseiten. Die Adressierung der LED beginnt bei 0. Die Adresse einer LED ist also deren Position auf dem Stripe minus 1. Es können sowohl einzelne Indices getrennt mit "," oder auch Bereiche verbunden mit "-" eingetragen werden. Fehlende LED werden mit einem "X" gekennzeichnet. Beispielsweise bedeutet "33-56" "LEDs 34 bis 57", "22-5" bedeutet "LEDs 23 bis 6 absteigend", und "13-0,4X,97-84" bedeutet "LEDs 14 bis 1, dann ein unbelegter Bereich in der Breite von 4 LED und anschließend noch die LEDs 98 bis 85". Die "X" sind vor allem im Bereich des Fernseherfußes oder der Einspeisung sinnvoll um die Positionsberechnung nicht durcheinanderzubringen. Die Aufzählung der LEDs geschieht generell von links nach rechts für die obere und untere Kante und von oben nach unten für die Seitenkanten.
- `led-inset-top`, `led-inset-bottom`, `led-inset-left`, `led-inset-right`: Da die Stripes aufgrund ihrer Struktur nicht an beliebigen Stellen getrennt werden können, kann es an den Ecken vorkommen, daß die Streifen entweder länger oder kürzer als die eigentlichen Bildschirmabmessungen sind. Das kann mit diesen Prozentwerten einkalkuliert werden. So bedeutet z.B. ein Wert von '3.5' daß der Stripe an dieser Kante um 3,5% des Bildbereiches kürzer ist als der Bildbereich, ein Wert von "-1.2" bedeutet einen 1,2% längeren Streifen an dieser Kante.
- `gamma-red`, `gamma-green`, `gamma-blue`: Legt die Gamma-Tabelle für die einzelnen Farben fest. Da die LED-Stripes einen linearen Farbraum umsetzen, ist eine vorherige Gamma-Korrektur für eine farbechte Wiederabe nötig. Zwar könnten alle Einzelfarben mit dem gleichen Gamma-Wert arbeiten, für eine Anpassung an LED-Toleranzen oder farbige Hintergründe können die Gamma-Werte für jede Farbe separat festgelegt werden um solche Verschiebungen ausgleichen zu können. Werte zwischen 1.6 und 1.8 sind brauchbar, testhalber kann man aber auch Werte zwischen 2.2 und 2.8, welche dem PAL- und NTSC-Farbraum entsprechen würden, ausprobieren.
- `blended-frames`: Zur Beruhigung des Farbeffektes kann man den Prozessor einen gleitenden Mittelwert über mehrere Abtastungen bilden lassen. Damit wird ein Flackern der LED bei schnellen Bildwechseln verhindert. Je mehr Abtastungen einbezogen werden um so sanfter erfolgen die Farbwechsel. Das muß man nach eigenem Geschmack austesten. Ein Wert von `0`oder `1` deaktiviert die Glättung.
- `overall-brightness`: Legt die Gesamthelligkeit der LEDs in Prozent fest. Gültig sind Werte von 0..100.  
- `intensity-red`: Legt die Einzelhelligkeit der roten LEDs in Prozent der Gesamthelligkeit fest. Gültig sind Werte von 0..100.  
- `intensity-green`: Legt die Einzelhelligkeit der grünen LEDs in Prozent der Gesamthelligkeit fest. Gültig sind Werte von 0..100.  
- `intensity-blue`: Legt die Einzelhelligkeit der blauen LEDs in Prozent der Gesamthelligkeit fest. Gültig sind Werte von 0..100.  
- `intensity-min-red`: Legt die Minimalhelligkeit der roten LEDs in Prozent der vollen Helligkeit fest. Gültig sind Werte von 0..100.  
- `intensity-min-green`: Legt die Minimalhelligkeit der grünen LEDs in Prozent der vollen Helligkeit fest. Gültig sind Werte von 0..100.  
- `intensity-min-blue`: Legt die Minimalhelligkeit der blauen LEDs in Prozent der vollen Helligkeit fest. Gültig sind Werte von 0..100.  

## ambi-tv erweitern

Aufgrund der komponentenbasierten Struktur von ambi-tv gestaltet sich die Erstellung eigener Komponenten (Quellen, Prozessoren und Sinks) relativ einfach. Aus der Datei `component.h`, wird ersichtlich, wie man diese in die Verwaltung einfügen kann. Die Komponenten selbst kommen nach `src/components`.

Die Grundidee dabei ist, daß jede Komponente nur eine bestimmte Anzahl Funktionen bedienen muß, welche in einer immer gleichen Struktur mi Funktionspointern zusammengefaßt werden. Ihre Konfiguration erfolgt über den Kommandozeilenmechanismus `int argc, char** argv` welcher es ermöglich, die komfortable Funktion `getopt_long` für die Parameterauswertung zu verwenden.

Neu geschriebene Komponenten müssen in `registrations.c` durch Hinzufügen zu Liste bekanntgemacht werden.

## Web-Interface

Die Steuerung von ambi-tv über Webinterface funktioniert von jedem beliebigen Gerät mit Web-Client (Browser, wget, curl o.Ä.) aus. Hier eine Beschreibung der Befehle und Parameter für das Webinterface (statt "raspi" die IP des Raspi, statt "port" den beim Start in der Kommandozeile als optionalen Parameter angegebenen Port [default 16384] und statt "color" die gewünschten Farben "red", "green" oder "blue" verwenden. "n" wird durch die gewünschten Ziffern ersetzt. Die Kombination mehrerer Parameter in einem Aufruf wird noch nicht unterstützt).
Um einen Wert abzufragen statt ihn zu setzen ist bei dem jeweiligen Aufruf hinter dem "=" nichts einzutragen. In diesem Fall antwortet ambi-tv statt mit "OK" oder "ERR" mit dem für diesen Parameter eingestellten Wert. "http://raspi:port?brightness=" würde dann zum Beispiel bei einer eingestellten Gesamthelligkeit von 90% mit "90" beantwortet werden.

*Konfigurationsdatei auslesen:*  
`http://raspi:port?getconfig`

Aus dieser Datei kann man Anzahl, Anordnung und Namen der implementierten Programme sowie die nach dem Start eingestellten Werte für Helligkeit, Intensität und Gammawert der Farben auslesen. Auch die Einstellungen der Audiokomponenten sind so ermittelbar.

*Modus setzen:*  
`http://raspi:port?mode=n`

Welche Modusnummer welches Programm aufruft und wieviele Modi es gibt, hängt von den Einträgen in der Config-Datei ab. Alle Werte, die größer als der maximal mögliche Modus sind schalten das Ambilight aus. Die Zählung beginnt dabei bei "0" für das erste Programm.

*Gesamthelligkeit setzen (0...100%):*  
`http://raspi:port?brightness=nnn`

*Intensität einer Farbe setzen (0...100%):*  
`http://raspi:port?intensity-color=nnn`

*Gamma-Wert einer Farbe setzen (0.00 ... 9.99):*  
`http://raspi:port?gamma-color=n.nn`

Nicht vergessen: statt "color" die Farben "red", "green" oder "blue" einsetzen.


*Audioempfindlichkeit setzen (0...1000%):*  
`http://raspi:port?sensitivity=nnn`

*Spektrum-Filter setzen (0 - 7):*  
`http://raspi:port?smoothing=n`

*Linear-Modus setzen (0, 1):*  
`http://raspi:port?linear=n`


## Tools

Für Linux-Receiver mit Neutrino und installierten Plugins FlexMenü, Input und MessageBox liegt in "tools/" das Script "ambi-tv-config", mit welchem man ambi-tv vom Receiver aus menügesteuert kontrollieren und parametrieren kann. Einige Screenshots der Menüs, welche einige Möglichkeiten der Steuerung demonstrieren sind [hier](doc/ambi-config.jpg) zusammengestellt. 
Receiver mit Neutrino und LUA-Unterstützung können das ebenfalls in "tools/" liegende LUA-Script verwenden.