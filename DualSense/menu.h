/* menu.h -- the DualSense's settings in the game's own menu: Pause (or the main menu) > Settings > Controls.
   Below the game's rows of that page come the DualSense's: switches (the game's own On/Off spinners) and sliders
   (the game's own option sliders), named and described in the game's language -- every language it has: the texts
   are added to its string tables (MyProfile.str, keys IDS_DS_..., files.h). While one of them is selected, the
   page shows its description and the controller's state (model, USB or Bluetooth, charge, firmware, how it
   vibrates, ABS as the brakes show it) where the game draws the controller's layout for its own rows. Three switches
   at the end run the tests of the vibration, the triggers and the lights. A change is used at once and kept in
   DualSense.ini.
   How (FH2 1.0.0.64): the page is XAML (UI.zip: Scenes/Anthem/A_ControllerOptions.xaml, changed by menu_xaml), bound
   to the C++ page ControllerOptions by property names. A view-model type lists its properties once, the first time
   it is used: the page's type does it in the function that adds its eight descriptors (found by the code around
   the name "InvertCameraLookBack"); that function is hooked, and ours adds the DualSense's after them: DS_<row>
   (ints for the switches, floats for the sliders), DS_Selected (the list's selected row, bound both ways),
   DS_ShowInfo (a bool: one of ours is selected) and DS_Status (the state's text). A descriptor is made like the
   game's (the InvertCameraLookBack one for ints, 0xED6E90's for floats: vtable, base object 0x143EB80), with our
   getter (out, desc, page: the value boxed with 0x4702D0 / 0x3F5990 / 0x336BA0, or an AVUI string, 0x1663220) and
   setter (desc, page, value: the int or float at +0x28). 0xCF9C60 (page, desc) tells the page a value changed. */
#pragma once

enum { L_EN, L_DE, L_FR, L_IT, L_ES, L_PT, L_PL, L_NL, L_RU, L_JA, L_KO, L_ZH, L_COUNT };
/* the game's string tables, media\stripped\stringtables\<this>.zip, and the language of each (cz and HU have the
   English text, as DEV and LOC) */
static const char *const kLocZip[] = {"EN", "GB", "DE", "FR", "IT", "ES", "MX", "br", "PL",
                                      "nl", "RU", "JP", "KO", "CHT", "cz", "HU", "DEV", "LOC"};
static const BYTE kLocLang[] = {L_EN, L_EN, L_DE, L_FR, L_IT, L_ES, L_ES, L_PT, L_PL,
                                L_NL, L_RU, L_JA, L_KO, L_ZH, L_EN, L_EN, L_EN, L_EN};
#define LOC_ZIPS 18
static volatile LONG g_lang = -1; /* the game's language: the string table it opened (L_*) */
static int g_menuOn;              /* the controller settings page has the DualSense's section (files.h) */

enum { R_SWITCH, R_SLIDER };
typedef struct {
    const char *key; /* DS_<key>, IDS_DS_<key>, IDS_DS_<key>_Desc */
    BYTE kind;
    float max;
    const char *label[L_COUNT];
    const char *desc[L_COUNT];
} MenuRow;

/* the rows, in their order on the page; texts: en de fr it es pt pl nl ru ja ko zh */
static const MenuRow kMenu[] = {
    {"Triggers", R_SWITCH, 1,
     {"Adaptive triggers", "Adaptive Trigger", "Gâchettes adaptatives", "Grilletti adattivi", "Gatillos adaptativos",
      "Gatilhos adaptáveis", "Adaptacyjne spusty", "Adaptieve triggers", "Адаптивные курки", "アダプティブトリガー",
      "적응형 트리거", "自適應扳機"},
     {"L2 is a brake pedal, R2 a light throttle. The tyres' grip, ABS at work or a locked wheel, wheel spin, gear "
      "changes, crashes, landings and the road are felt in them.",
      "L2 ist ein Bremspedal, R2 ein leichtes Gaspedal. Reifenhaftung, arbeitendes ABS oder ein blockierendes Rad, "
      "durchdrehende Räder, Gangwechsel, Unfälle, Landungen und die Fahrbahn sind darin spürbar.",
      "L2 devient une pédale de frein, R2 un accélérateur léger. L'adhérence des pneus, l'ABS en action ou une roue "
      "bloquée, le patinage, les changements de rapport, les chocs, les atterrissages et la route s'y font sentir.",
      "L2 diventa un pedale del freno, R2 un acceleratore leggero. Aderenza degli pneumatici, ABS in funzione o ruota "
      "bloccata, pattinamento, cambi marcia, urti, atterraggi e strada si sentono nei grilletti.",
      "L2 es un pedal de freno y R2 un acelerador suave. Se sienten el agarre de los neumáticos, el ABS en acción o una "
      "rueda bloqueada, el patinaje, los cambios de marcha, los choques, los aterrizajes y la carretera.",
      "L2 vira um pedal de freio e R2 um acelerador leve. Você sente a aderência dos pneus, o ABS atuando ou uma roda "
      "travada, a patinação, as trocas de marcha, as batidas, as aterrissagens e a pista.",
      "L2 to pedał hamulca, R2 lekki pedał gazu. Czuć w nich przyczepność opon, pracę ABS lub zablokowane koło, "
      "buksowanie, zmiany biegów, zderzenia, lądowania i nawierzchnię.",
      "L2 is een rempedaal, R2 een licht gaspedaal. Je voelt de grip van de banden, werkende ABS of een blokkerend "
      "wiel, doorslippende wielen, schakelen, botsingen, landingen en het wegdek.",
      "L2 — педаль тормоза, R2 — лёгкий газ. В них чувствуются сцепление шин, работа ABS или блокировка колеса, "
      "пробуксовка, переключения передач, удары, приземления и дорога.",
      "L2はブレーキペダル、R2は軽いアクセルになります。タイヤのグリップ、ABSの作動やホイールロック、ホイールスピン、"
      "シフトチェンジ、衝突、着地、路面を感じられます。",
      "L2는 브레이크 페달, R2는 가벼운 가속 페달이 됩니다. 타이어 접지력, ABS 작동 또는 바퀴 잠김, 휠스핀, 기어 변속, "
      "충돌, 착지, 노면이 느껴집니다.",
      "L2 成為煞車踏板，R2 成為輕盈的油門。可感受到輪胎抓地力、ABS 作動或車輪鎖死、輪胎空轉、換檔、碰撞、落地與路面。"}},
    {"TriggerStrength", R_SLIDER, 1,
     {"Trigger strength", "Trigger-Stärke", "Force des gâchettes", "Forza dei grilletti", "Fuerza de los gatillos",
      "Força dos gatilhos", "Siła spustów", "Kracht van triggers", "Сила курков", "トリガーの強さ", "트리거 강도",
      "扳機強度"},
     {"How hard the triggers push back and vibrate.", "Wie stark die Trigger Widerstand leisten und vibrieren.",
      "La force de la résistance et des vibrations des gâchettes.", "Quanto i grilletti oppongono resistenza e vibrano.",
      "La fuerza con la que los gatillos oponen resistencia y vibran.", "A força da resistência e da vibração dos gatilhos.",
      "Jak mocno spusty stawiają opór i wibrują.", "Hoe sterk de triggers tegendruk geven en trillen.",
      "Насколько сильно курки сопротивляются и вибрируют.", "トリガーの抵抗と振動の強さです。", "트리거의 저항과 진동 세기입니다.",
      "扳機阻力與震動的強度。"}},
    {"Vibration", R_SLIDER, 2,
     {"DualSense vibration", "DualSense-Vibration", "Vibrations DualSense", "Vibrazione DualSense",
      "Vibración del DualSense", "Vibração do DualSense", "Wibracje DualSense", "DualSense-trilling", "Вибрация DualSense",
      "DualSenseの振動", "DualSense 진동", "DualSense 震動"},
     {"The strength of all vibration on the DualSense, up to 200%.", "Die Stärke aller Vibrationen des DualSense, bis 200 %.",
      "L'intensité de toutes les vibrations de la DualSense, jusqu'à 200 %.",
      "L'intensità di tutte le vibrazioni del DualSense, fino al 200%.",
      "La intensidad de toda la vibración del DualSense, hasta el 200 %.",
      "A intensidade de toda a vibração do DualSense, até 200%.", "Siła wszystkich wibracji DualSense, do 200%.",
      "De sterkte van alle trillingen van de DualSense, tot 200%.", "Сила всей вибрации DualSense, до 200 %.",
      "DualSenseのすべての振動の強さ（最大200%）。", "DualSense의 모든 진동 세기 (최대 200%).", "DualSense 所有震動的強度，最高 200%。"}},
    {"HDHaptics", R_SWITCH, 1,
     {"HD haptics (USB)", "HD-Haptik (USB)", "Retour haptique HD (USB)", "Feedback aptico HD (USB)",
      "Vibración háptica HD (USB)", "Resposta tátil HD (USB)", "Haptyka HD (USB)", "HD-haptiek (USB)", "HD-вибрация (USB)",
      "HD振動 (USB)", "HD 햅틱 (USB)", "HD 觸覺回饋 (USB)"},
     {"Over a USB cable the controller's own haptic actuators play the vibration: finer, with the engine and the road "
      "in it. Off: the classic rumble.",
      "Über ein USB-Kabel erzeugen die haptischen Aktuatoren des Controllers die Vibration: feiner, mit Motor und "
      "Fahrbahn. Aus: klassisches Rumble.",
      "Par câble USB, les actionneurs haptiques de la manette produisent les vibrations : plus fines, avec le moteur et "
      "la route. Désactivé : vibrations classiques.",
      "Con il cavo USB la vibrazione è riprodotta dagli attuatori aptici del controller: più fine, con motore e strada. "
      "Disattivato: vibrazione classica.",
      "Con cable USB, los actuadores hápticos del mando reproducen la vibración: más fina, con el motor y la carretera. "
      "Desactivado: vibración clásica.",
      "Pelo cabo USB, os atuadores táteis do controle reproduzem a vibração: mais fina, com o motor e a pista. "
      "Desligado: vibração clássica.",
      "Przez kabel USB wibracje odtwarzają siłowniki haptyczne kontrolera: subtelniej, z silnikiem i nawierzchnią. "
      "Wyłączone: klasyczne wibracje.",
      "Via een USB-kabel spelen de haptische actuatoren van de controller de trillingen af: fijner, met motor en "
      "wegdek. Uit: klassieke trilling.",
      "По USB-кабелю вибрацию играют собственные вибромоторы контроллера: тоньше, с двигателем и дорогой. Выкл.: "
      "классическая вибрация.",
      "USBケーブル接続時はコントローラーのハプティックアクチュエーターで振動を再生します。より繊細で、エンジンや路面も"
      "感じられます。オフ：従来の振動。",
      "USB 케이블 연결 시 컨트롤러의 햅틱 액추에이터가 진동을 재생합니다. 더 섬세하며 엔진과 노면이 느껴집니다. "
      "끄기: 기존 진동.",
      "使用 USB 線時，由控制器的觸覺致動器播放震動：更細膩，能感受到引擎與路面。關閉：傳統震動。"}},
    {"Engine", R_SLIDER, 1,
     {"Engine feel", "Motor spüren", "Ressenti moteur", "Sensazione motore", "Sensación del motor", "Sensação do motor",
      "Wyczucie silnika", "Motorgevoel", "Двигатель в вибрации", "エンジンの振動", "엔진 느낌", "引擎震動"},
     {"The engine's hum at its revs, the rev limiter and gear changes in the vibration.",
      "Das Brummen des Motors nach Drehzahl, der Drehzahlbegrenzer und Gangwechsel in der Vibration.",
      "Le grondement du moteur selon le régime, le rupteur et les changements de rapport dans les vibrations.",
      "Il rombo del motore in base ai giri, il limitatore e i cambi marcia nella vibrazione.",
      "El zumbido del motor según las revoluciones, el limitador y los cambios de marcha en la vibración.",
      "O ronco do motor conforme a rotação, o limitador e as trocas de marcha na vibração.",
      "Buczenie silnika zależne od obrotów, ogranicznik obrotów i zmiany biegów w wibracjach.",
      "Het brommen van de motor naar toerental, de toerenbegrenzer en schakelen in de trillingen.",
      "Гул двигателя по оборотам, отсечка и переключения передач в вибрации.",
      "回転数に応じたエンジンの唸り、レブリミッター、シフトチェンジを振動で再現します。",
      "회전수에 따른 엔진 울림, 레브 리미터, 기어 변속을 진동으로 느낍니다.", "依轉速的引擎低鳴、斷油與換檔都會反映在震動中。"}},
    {"Menus", R_SLIDER, 1,
     {"Menu haptics", "Menü-Haptik", "Retour haptique des menus", "Feedback aptico nei menu", "Respuesta háptica en menús",
      "Resposta tátil nos menus", "Haptyka w menu", "Menuhaptiek", "Вибро-отклик меню", "メニューの振動", "메뉴 햅틱",
      "選單觸覺回饋"},
     {"A tick for every move in the menus, a click for every choice, pages opening and the prize drum ticking by, as on "
      "a PS5.",
      "Ein Ticken bei jeder Bewegung im Menü, ein Klicken bei jeder Auswahl, sich öffnende Seiten und die tickende "
      "Preistrommel, wie auf der PS5.",
      "Un tic à chaque déplacement dans les menus, un clic à chaque choix, l'ouverture des pages et le tambour des prix "
      "qui défile, comme sur PS5.",
      "Un tic a ogni spostamento nei menu, un clic a ogni scelta, l'apertura delle pagine e il tamburo dei premi che "
      "scorre, come su PS5.",
      "Un tic con cada movimiento en los menús, un clic con cada elección, páginas que se abren y el tambor de premios "
      "girando, como en PS5.",
      "Um toque a cada movimento nos menus, um clique a cada escolha, páginas abrindo e o tambor de prêmios girando, "
      "como no PS5.",
      "Tyknięcie przy każdym ruchu w menu, kliknięcie przy każdym wyborze, otwieranie stron i bęben nagród, jak na PS5.",
      "Een tik bij elke beweging in de menu's, een klik bij elke keuze, openende pagina's en de prijzentrommel, zoals "
      "op de PS5.",
      "Щелчок на каждое перемещение в меню, клик на каждый выбор, открытие страниц и барабан призов — как на PS5.",
      "メニューの移動ごとのクリック感、決定時のクリック、ページの切り替え、賞品ドラムの回転を、PS5のように感じられます。",
      "메뉴 이동마다 딸깍임, 선택할 때마다 클릭, 페이지 전환과 상품 드럼 회전을 PS5처럼 느낍니다.",
      "選單中每次移動的輕觸、每次選擇的點擊、頁面開啟與獎品轉盤的轉動，都像在 PS5 上一樣。"}},
    {"Events", R_SLIDER, 1,
     {"Game events", "Spielereignisse", "Événements du jeu", "Eventi di gioco", "Eventos del juego", "Eventos do jogo",
      "Wydarzenia w grze", "Spelgebeurtenissen", "События игры", "ゲームイベント", "게임 이벤트", "遊戲事件"},
     {"Skills and combos, near misses, checkpoints, countdowns, landings after jumps, level-ups and prizes in the "
      "vibration.",
      "Fertigkeiten und Kombos, Beinahe-Unfälle, Checkpoints, Countdowns, Landungen nach Sprüngen, Levelaufstiege und "
      "Preise in der Vibration.",
      "Compétences et combos, frôlements, points de passage, comptes à rebours, atterrissages après les sauts, niveaux "
      "et prix dans les vibrations.",
      "Abilità e combo, sorpassi al limite, checkpoint, conti alla rovescia, atterraggi dopo i salti, nuovi livelli e "
      "premi nella vibrazione.",
      "Habilidades y combos, casi choques, puntos de control, cuentas atrás, aterrizajes tras saltos, subidas de nivel "
      "y premios en la vibración.",
      "Habilidades e combos, quase colisões, checkpoints, contagens regressivas, aterrissagens após saltos, subidas de "
      "nível e prêmios na vibração.",
      "Umiejętności i kombo, niemal zderzenia, punkty kontrolne, odliczania, lądowania po skokach, awanse i nagrody w "
      "wibracjach.",
      "Vaardigheden en combo's, bijna-botsingen, checkpoints, aftellen, landingen na sprongen, levelstijgingen en "
      "prijzen in de trillingen.",
      "Навыки и комбо, почти-столкновения, чекпоинты, отсчёт, приземления после прыжков, новые уровни и призы в "
      "вибрации.",
      "スキルとコンボ、ニアミス、チェックポイント、カウントダウン、ジャンプ後の着地、レベルアップや賞品を振動で感じ"
      "られます。",
      "스킬과 콤보, 아슬아슬한 회피, 체크포인트, 카운트다운, 점프 후 착지, 레벨 업과 상품을 진동으로 느낍니다.",
      "技巧與連擊、驚險擦身、檢查點、倒數、跳躍後落地、升級與獎品都會反映在震動中。"}},
    {"Surface", R_SLIDER, 1,
     {"Road surface", "Fahrbahnbelag", "Revêtement de la route", "Fondo stradale", "Superficie de la carretera",
      "Superfície da pista", "Nawierzchnia", "Wegdek", "Покрытие дороги", "路面", "노면", "路面"},
     {"Gravel, dirt, grass, sand, cobbles and kerbs each have their own feel: in the vibration on the side the wheels "
      "are on, and in the triggers.",
      "Schotter, Erde, Gras, Sand, Kopfsteinpflaster und Randsteine fühlen sich jeweils anders an: in der Vibration "
      "auf der Seite der Räder und in den Triggern.",
      "Gravier, terre, herbe, sable, pavés et vibreurs ont chacun leur sensation : dans les vibrations, du côté des "
      "roues, et dans les gâchettes.",
      "Ghiaia, sterrato, erba, sabbia, pavé e cordoli hanno ciascuno la propria sensazione: nella vibrazione dal lato "
      "delle ruote e nei grilletti.",
      "Grava, tierra, hierba, arena, adoquines y pianos se sienten distintos: en la vibración del lado de las ruedas y "
      "en los gatillos.",
      "Cascalho, terra, grama, areia, paralelepípedos e zebras têm sensações próprias: na vibração do lado das rodas e "
      "nos gatilhos.",
      "Żwir, ziemia, trawa, piasek, bruk i tarki mają własne odczucie: w wibracjach po stronie kół i w spustach.",
      "Grind, aarde, gras, zand, kinderkopjes en curbs voelen elk anders: in de trillingen aan de kant van de wielen "
      "en in de triggers.",
      "У гравия, грунта, травы, песка, брусчатки и поребриков своё ощущение: в вибрации с той стороны, где колёса, "
      "и в курках.",
      "砂利、土、芝生、砂、石畳、縁石はそれぞれ異なる感触です。タイヤのある側の振動とトリガーで感じられます。",
      "자갈, 흙, 잔디, 모래, 자갈길, 연석마다 다른 느낌이 납니다. 바퀴가 있는 쪽의 진동과 트리거로 느낍니다.",
      "碎石、泥土、草地、沙地、石板路與路緣石各有不同觸感：在車輪所在側的震動與扳機中感受。"}},
    {"Gyro", R_SWITCH, 1,
     {"Gyro steering", "Gyro-Lenkung", "Direction gyroscopique", "Sterzo giroscopico", "Dirección por giroscopio",
      "Direção por giroscópio", "Sterowanie żyroskopem", "Gyrosturing", "Руление наклоном", "ジャイロ操作", "자이로 조향",
      "體感轉向"},
     {"The mic button turns steering by tilting the controller like a wheel on and off; its light shows it is on.",
      "Die Mikrofontaste schaltet das Lenken durch Neigen des Controllers wie ein Lenkrad ein und aus; ihr Licht zeigt, "
      "dass es aktiv ist.",
      "La touche micro active ou désactive la direction en inclinant la manette comme un volant ; son voyant indique "
      "qu'elle est active.",
      "Il tasto del microfono attiva e disattiva la sterzata inclinando il controller come un volante; la sua luce "
      "indica che è attiva.",
      "El botón del micrófono activa y desactiva la dirección inclinando el mando como un volante; su luz indica que "
      "está activa.",
      "O botão do microfone liga e desliga a direção inclinando o controle como um volante; a luz dele mostra que está "
      "ativa.",
      "Przycisk mikrofonu włącza i wyłącza sterowanie przechylaniem kontrolera jak kierownicą; jego światło pokazuje, "
      "że jest włączone.",
      "De microfoonknop zet sturen door de controller als een stuur te kantelen aan en uit; het lampje toont dat het "
      "aan staat.",
      "Кнопка микрофона включает и выключает руление наклоном контроллера, как рулём; её подсветка показывает, что "
      "включено.",
      "マイクボタンで、コントローラーをハンドルのように傾けて操作する機能をオン・オフします。ランプ点灯中はオンです。",
      "마이크 버튼으로 컨트롤러를 핸들처럼 기울여 조향하는 기능을 켜고 끕니다. 표시등이 켜져 있으면 작동 중입니다.",
      "麥克風按鈕可開關像方向盤一樣傾斜控制器來轉向的功能；燈亮表示已開啟。"}},
    {"LightBar", R_SLIDER, 1,
     {"Light bar", "Lichtleiste", "Barre lumineuse", "Barra luminosa", "Barra de luz", "Barra de luz", "Pasek świetlny",
      "Lichtbalk", "Световая панель", "ライトバー", "라이트 바", "燈條"},
     {"The light bar's brightness. It flashes with impacts and sliding tyres, and blinks red when the battery is low.",
      "Die Helligkeit der Lichtleiste. Sie blitzt bei Stößen und rutschenden Reifen auf und blinkt rot bei schwachem Akku.",
      "La luminosité de la barre lumineuse. Elle s'illumine lors des chocs et des glissades, et clignote en rouge quand "
      "la batterie est faible.",
      "La luminosità della barra luminosa. Lampeggia con urti e pneumatici che slittano, e diventa rossa con la "
      "batteria scarica.",
      "El brillo de la barra de luz. Destella con los golpes y los derrapes, y parpadea en rojo con la batería baja.",
      "O brilho da barra de luz. Ela pisca com impactos e derrapagens, e fica vermelha com a bateria fraca.",
      "Jasność paska świetlnego. Błyska przy uderzeniach i poślizgach, a przy słabej baterii miga na czerwono.",
      "De helderheid van de lichtbalk. Hij flitst bij klappen en slippende banden en knippert rood bij een lege batterij.",
      "Яркость световой панели. Она вспыхивает от ударов и скольжения шин и мигает красным при низком заряде.",
      "ライトバーの明るさです。衝撃やタイヤのスライドで光り、バッテリー残量が少ないと赤く点滅します。",
      "라이트 바의 밝기입니다. 충돌과 타이어 미끄러짐에 반짝이며, 배터리가 부족하면 빨간색으로 깜박입니다.",
      "燈條的亮度。碰撞與輪胎打滑時會閃爍，電量不足時閃紅燈。"}},
    {"RevLights", R_SWITCH, 1,
     {"Rev counter lights", "Drehzahlanzeige (LEDs)", "Compte-tours (voyants)", "Contagiri sui LED",
      "Cuentarrevoluciones (luces)", "Conta-giros (luzes)", "Obrotomierz (diody)", "Toerenteller (lampjes)",
      "Тахометр на индикаторах", "回転数ランプ", "회전수 표시등", "轉速指示燈"},
     {"The five player lights under the touchpad show the engine's revs while driving.",
      "Die fünf Spieler-LEDs unter dem Touchpad zeigen beim Fahren die Motordrehzahl.",
      "Les cinq voyants joueur sous le pavé tactile indiquent le régime moteur pendant la conduite.",
      "I cinque LED giocatore sotto il touchpad mostrano i giri del motore durante la guida.",
      "Las cinco luces de jugador bajo el panel táctil muestran las revoluciones del motor al conducir.",
      "As cinco luzes de jogador abaixo do touchpad mostram a rotação do motor ao dirigir.",
      "Pięć diod gracza pod panelem dotykowym pokazuje obroty silnika podczas jazdy.",
      "De vijf spelerlampjes onder het touchpad tonen tijdens het rijden het toerental.",
      "Пять индикаторов игрока под тачпадом показывают обороты двигателя во время езды.",
      "タッチパッド下の5つのプレイヤーランプが、走行中のエンジン回転数を表示します。",
      "터치패드 아래 플레이어 표시등 5개가 주행 중 엔진 회전수를 보여 줍니다.", "觸控板下方的五個玩家指示燈會在行駛時顯示引擎轉速。"}},
    {"ComboLights", R_SWITCH, 1,
     {"Combo lights", "Kombo-Anzeige (LEDs)", "Combo sur les voyants", "Combo sui LED", "Combo en las luces",
      "Combo nas luzes", "Kombo na diodach", "Combo op lampjes", "Комбо на индикаторах", "コンボランプ", "콤보 표시등",
      "連擊指示燈"},
     {"The player lights show the skill chain: one more light for every skill; all of them blink when it is banked.",
      "Die Spieler-LEDs zeigen die Fertigkeitskette: eine LED mehr pro Fertigkeit; alle blinken, wenn sie gesichert wird.",
      "Les voyants joueur montrent la chaîne de compétences : un voyant de plus par compétence ; tous clignotent quand "
      "elle est validée.",
      "I LED giocatore mostrano la catena di abilità: un LED in più per ogni abilità; lampeggiano tutti quando viene "
      "incassata.",
      "Las luces de jugador muestran la cadena de habilidades: una luz más por habilidad; todas parpadean al asegurarla.",
      "As luzes de jogador mostram a sequência de habilidades: uma luz a mais por habilidade; todas piscam quando ela é "
      "garantida.",
      "Diody gracza pokazują łańcuch umiejętności: jedna dioda więcej za każdą; wszystkie migają, gdy punkty zostaną "
      "zaliczone.",
      "De spelerlampjes tonen de vaardigheidsketen: een lampje meer per vaardigheid; ze knipperen allemaal als die wordt "
      "verzilverd.",
      "Индикаторы игрока показывают цепочку навыков: ещё один огонёк за каждый навык; все мигают, когда очки "
      "засчитаны.",
      "プレイヤーランプがスキルチェーンを表示します。スキルごとに1つずつ点灯し、ポイント確定時に全て点滅します。",
      "플레이어 표시등이 스킬 체인을 보여 줍니다. 스킬마다 하나씩 켜지고, 점수가 확정되면 모두 깜박입니다.",
      "玩家指示燈會顯示技巧連鎖：每個技巧多亮一顆；分數入帳時全部閃爍。"}},
    {"Icons", R_SWITCH, 1,
     {"PlayStation button icons", "PlayStation-Tastensymbole", "Icônes des touches PlayStation",
      "Icone dei tasti PlayStation", "Iconos de botones PlayStation", "Ícones dos botões PlayStation",
      "Ikony przycisków PlayStation", "PlayStation-knoppictogrammen", "Иконки кнопок PlayStation",
      "PlayStationのボタンアイコン", "PlayStation 버튼 아이콘", "PlayStation 按鈕圖示"},
     {"The game's button icons and the controller drawing with PlayStation symbols, when a DualSense is connected as "
      "the game starts. Takes effect at the next start.",
      "Die Tastensymbole des Spiels und die Controller-Grafik mit PlayStation-Symbolen, wenn beim Spielstart ein "
      "DualSense angeschlossen ist. Wirkt ab dem nächsten Start.",
      "Les icônes des touches et le schéma de la manette avec les symboles PlayStation, si une DualSense est connectée "
      "au lancement du jeu. Effectif au prochain lancement.",
      "Le icone dei tasti e il disegno del controller con i simboli PlayStation, se all'avvio del gioco è collegato un "
      "DualSense. Attivo dal prossimo avvio.",
      "Los iconos de botones y el dibujo del mando con símbolos PlayStation, si hay un DualSense conectado al iniciar "
      "el juego. Se aplica en el próximo inicio.",
      "Os ícones dos botões e o desenho do controle com símbolos PlayStation, se houver um DualSense conectado ao "
      "iniciar o jogo. Vale a partir do próximo início.",
      "Ikony przycisków i rysunek kontrolera z symbolami PlayStation, jeśli przy uruchomieniu gry podłączony jest "
      "DualSense. Działa od następnego uruchomienia.",
      "De knoppictogrammen en de controllertekening met PlayStation-symbolen, als er bij het starten een DualSense is "
      "aangesloten. Werkt vanaf de volgende start.",
      "Иконки кнопок игры и схема контроллера с символами PlayStation, если DualSense подключён при запуске игры. Со "
      "следующего запуска.",
      "ゲーム起動時にDualSenseが接続されていると、ボタンアイコンとコントローラー図がPlayStationの記号になります。"
      "次回起動時から有効です。",
      "게임 시작 시 DualSense가 연결되어 있으면 버튼 아이콘과 컨트롤러 그림이 PlayStation 기호로 바뀝니다. 다음 "
      "실행부터 적용됩니다.",
      "遊戲啟動時若已連接 DualSense，按鈕圖示與控制器圖會改為 PlayStation 符號。下次啟動時生效。"}},
    {"TestVibration", R_SWITCH, 1,
     {"Test: vibration", "Test: Vibration", "Test : vibrations", "Test: vibrazione", "Prueba: vibración",
      "Teste: vibração", "Test: wibracje", "Test: trilling", "Проверка: вибрация", "テスト: 振動", "테스트: 진동",
      "測試：震動"},
     {"Switch on to feel the left side, the right side, the menu ticks from left to right, a click, a counter and a "
      "landing.",
      "Einschalten, um linke und rechte Seite, das Menü-Ticken von links nach rechts, einen Klick, einen Zähler und "
      "eine Landung zu spüren.",
      "Activez pour sentir le côté gauche, le côté droit, les tics des menus de gauche à droite, un clic, un compteur "
      "et un atterrissage.",
      "Attiva per sentire il lato sinistro, il lato destro, i tic dei menu da sinistra a destra, un clic, un contatore "
      "e un atterraggio.",
      "Actívalo para sentir el lado izquierdo, el derecho, los tics del menú de izquierda a derecha, un clic, un "
      "contador y un aterrizaje.",
      "Ligue para sentir o lado esquerdo, o direito, os toques do menu da esquerda para a direita, um clique, um "
      "contador e uma aterrissagem.",
      "Włącz, aby poczuć lewą stronę, prawą stronę, tyknięcia menu od lewej do prawej, kliknięcie, licznik i lądowanie.",
      "Zet aan om de linkerkant, de rechterkant, menutikken van links naar rechts, een klik, een teller en een landing "
      "te voelen.",
      "Включите, чтобы почувствовать левую сторону, правую, щелчки меню слева направо, клик, счётчик и приземление.",
      "オンにすると、左側、右側、左から右へのメニュークリック、決定クリック、カウンター、着地を体験できます。",
      "켜면 왼쪽, 오른쪽, 왼쪽에서 오른쪽으로의 메뉴 딸깍임, 클릭, 카운터, 착지를 느낄 수 있습니다.",
      "開啟後可感受左側、右側、由左至右的選單輕觸、點擊、計數器與落地。"}},
    {"TestTriggers", R_SWITCH, 1,
     {"Test: triggers", "Test: Trigger", "Test : gâchettes", "Test: grilletti", "Prueba: gatillos", "Teste: gatilhos",
      "Test: spusty", "Test: triggers", "Проверка: курки", "テスト: トリガー", "테스트: 트리거", "測試：扳機"},
     {"Switch on, then press L2 and R2: the pedals, ABS, a locked wheel, wheel spin, gravel, a kerb and gear changes, "
      "1.5 seconds each.",
      "Einschalten, dann L2 und R2 drücken: Pedale, ABS, blockiertes Rad, durchdrehende Räder, Schotter, Randstein und "
      "Gangwechsel, je 1,5 Sekunden.",
      "Activez, puis appuyez sur L2 et R2 : pédales, ABS, roue bloquée, patinage, gravier, vibreur et changements de "
      "rapport, 1,5 seconde chacun.",
      "Attiva, poi premi L2 e R2: pedali, ABS, ruota bloccata, pattinamento, ghiaia, cordolo e cambi marcia, 1,5 "
      "secondi ciascuno.",
      "Actívalo y pulsa L2 y R2: pedales, ABS, rueda bloqueada, patinaje, grava, piano y cambios de marcha, 1,5 "
      "segundos cada uno.",
      "Ligue e pressione L2 e R2: pedais, ABS, roda travada, patinação, cascalho, zebra e trocas de marcha, 1,5 segundo "
      "cada.",
      "Włącz, a potem naciskaj L2 i R2: pedały, ABS, zablokowane koło, buksowanie, żwir, tarka i zmiany biegów, po 1,5 "
      "sekundy.",
      "Zet aan en druk L2 en R2 in: pedalen, ABS, blokkerend wiel, doorslippen, grind, curb en schakelen, elk 1,5 "
      "seconde.",
      "Включите и нажимайте L2 и R2: педали, ABS, заблокированное колесо, пробуксовка, гравий, поребрик и "
      "переключения, по 1,5 секунды.",
      "オンにしてL2とR2を押してください。ペダル、ABS、ホイールロック、ホイールスピン、砂利、縁石、シフトチェンジを"
      "各1.5秒ずつ体験できます。",
      "켠 다음 L2와 R2를 누르세요. 페달, ABS, 바퀴 잠김, 휠스핀, 자갈, 연석, 기어 변속이 각각 1.5초씩 재생됩니다.",
      "開啟後按住 L2 與 R2：踏板、ABS、車輪鎖死、輪胎空轉、碎石、路緣石與換檔，各 1.5 秒。"}},
    {"TestLights", R_SWITCH, 1,
     {"Test: lights", "Test: Beleuchtung", "Test : lumières", "Test: luci", "Prueba: luces", "Teste: luzes",
      "Test: podświetlenie", "Test: verlichting", "Проверка: подсветка", "テスト: ライト", "테스트: 조명", "測試：燈光"},
     {"Switch on to see the light bar run through its colours and the player lights move.",
      "Einschalten, um die Lichtleiste durch ihre Farben laufen und die Spieler-LEDs wandern zu sehen.",
      "Activez pour voir la barre lumineuse parcourir ses couleurs et les voyants joueur défiler.",
      "Attiva per vedere la barra luminosa cambiare colore e i LED giocatore scorrere.",
      "Actívalo para ver la barra de luz recorrer sus colores y moverse las luces de jugador.",
      "Ligue para ver a barra de luz percorrer suas cores e as luzes de jogador se moverem.",
      "Włącz, aby zobaczyć, jak pasek świetlny zmienia kolory, a diody gracza się przesuwają.",
      "Zet aan om de lichtbalk door zijn kleuren te zien lopen en de spelerlampjes te zien bewegen.",
      "Включите, чтобы световая панель прошла по цветам, а индикаторы игрока пробежали огоньком.",
      "オンにすると、ライトバーが色を切り替え、プレイヤーランプが流れるように光ります。",
      "켜면 라이트 바가 여러 색으로 바뀌고 플레이어 표시등이 움직입니다.", "開啟後燈條會循環顯示各種顏色，玩家指示燈也會流動。"}},
};
#define MENU_ROWS ((int)(sizeof kMenu / sizeof kMenu[0]))
#define ROW_TEST_VIBRATION (MENU_ROWS - 3)

/* the state's words: not connected, battery, charging, full, firmware, vibration, HD haptics, rumble emulation, off,
   ABS on, ABS off, the three tests running */
enum { W_NOTCONN, W_BATTERY, W_CHARGING, W_FULL, W_FIRMWARE, W_VIBRATION, W_HD, W_RUMBLE, W_OFF, W_ABS_ON, W_ABS_OFF,
       W_TEST_VIB, W_TEST_TRIG, W_TEST_LIGHTS, W_COUNT };
static const char *const kWords[L_COUNT][W_COUNT] = {
    {"Not connected", "Battery", "charging", "full", "Firmware", "Vibration", "HD haptics", "rumble emulation", "off",
     "ABS: on (as the brakes show it)", "ABS: off (as the brakes show it)", "Testing the vibration...",
     "Testing the triggers: press L2 and R2", "Testing the lights..."},
    {"Nicht verbunden", "Akku", "lädt", "voll", "Firmware", "Vibration", "HD-Haptik", "Rumble-Emulation", "aus",
     "ABS: an (laut Bremsen)", "ABS: aus (laut Bremsen)", "Vibrationstest läuft...", "Trigger-Test: L2 und R2 drücken",
     "Beleuchtungstest läuft..."},
    {"Non connectée", "Batterie", "en charge", "pleine", "Micrologiciel", "Vibrations", "retour haptique HD",
     "émulation des vibrations", "désactivées", "ABS : activé (d'après les freins)", "ABS : désactivé (d'après les freins)",
     "Test des vibrations...", "Test des gâchettes : appuyez sur L2 et R2", "Test des lumières..."},
    {"Non collegato", "Batteria", "in carica", "carica", "Firmware", "Vibrazione", "feedback aptico HD",
     "emulazione vibrazione", "disattivata", "ABS: attivo (dai freni)", "ABS: disattivo (dai freni)",
     "Test della vibrazione...", "Test dei grilletti: premi L2 e R2", "Test delle luci..."},
    {"No conectado", "Batería", "cargando", "llena", "Firmware", "Vibración", "háptica HD", "emulación de vibración",
     "desactivada", "ABS: activado (según los frenos)", "ABS: desactivado (según los frenos)", "Probando la vibración...",
     "Prueba de gatillos: pulsa L2 y R2", "Probando las luces..."},
    {"Não conectado", "Bateria", "carregando", "cheia", "Firmware", "Vibração", "resposta tátil HD", "emulação de vibração",
     "desligada", "ABS: ligado (pelos freios)", "ABS: desligado (pelos freios)", "Testando a vibração...",
     "Teste dos gatilhos: pressione L2 e R2", "Testando as luzes..."},
    {"Nie podłączono", "Bateria", "ładowanie", "pełna", "Oprogramowanie", "Wibracje", "haptyka HD", "emulacja wibracji",
     "wyłączone", "ABS: włączony (według hamulców)", "ABS: wyłączony (według hamulców)", "Test wibracji...",
     "Test spustów: naciskaj L2 i R2", "Test podświetlenia..."},
    {"Niet verbonden", "Batterij", "laadt op", "vol", "Firmware", "Trilling", "HD-haptiek", "trillingsemulatie", "uit",
     "ABS: aan (volgens de remmen)", "ABS: uit (volgens de remmen)", "Trillingstest...", "Triggertest: druk L2 en R2 in",
     "Verlichtingstest..."},
    {"Не подключён", "Заряд", "заряжается", "полный", "Прошивка", "Вибрация", "HD-вибрация", "эмуляция моторов",
     "выключена", "ABS: есть (по тормозам)", "ABS: нет (по тормозам)", "Проверка вибрации...",
     "Проверка курков: нажимайте L2 и R2", "Проверка подсветки..."},
    {"未接続", "バッテリー", "充電中", "満充電", "ファームウェア", "振動", "HD振動", "振動エミュレーション", "オフ",
     "ABS: オン（ブレーキから判定）", "ABS: オフ（ブレーキから判定）", "振動をテスト中...", "トリガーテスト: L2とR2を押してください",
     "ライトをテスト中..."},
    {"연결 안 됨", "배터리", "충전 중", "완충", "펌웨어", "진동", "HD 햅틱", "진동 에뮬레이션", "끔", "ABS: 켜짐 (브레이크로 판단)",
     "ABS: 꺼짐 (브레이크로 판단)", "진동 테스트 중...", "트리거 테스트: L2와 R2를 누르세요", "조명 테스트 중..."},
    {"未連接", "電量", "充電中", "已充滿", "韌體", "震動", "HD 觸覺回饋", "震動模擬", "關閉", "ABS：開啟（依煞車判斷）",
     "ABS：關閉（依煞車判斷）", "震動測試中...", "扳機測試：請按住 L2 與 R2", "燈光測試中..."},
};

/* the string tables' keys and one language's texts (files.h), in the same order */
static char g_menuKeyBuf[MENU_ROWS * 2][48];
static const char *g_menuKeys[MENU_ROWS * 2];
static const char *g_menuTexts[L_COUNT][MENU_ROWS * 2];

static const char *const *menu_keys(int *n)
{
    if (!g_menuKeys[0])
        for (int i = 0; i < MENU_ROWS; i++) {
            wsprintfA(g_menuKeyBuf[2 * i], "IDS_DS_%s", kMenu[i].key);
            wsprintfA(g_menuKeyBuf[2 * i + 1], "IDS_DS_%s_Desc", kMenu[i].key);
            g_menuKeys[2 * i] = g_menuKeyBuf[2 * i], g_menuKeys[2 * i + 1] = g_menuKeyBuf[2 * i + 1];
        }
    *n = MENU_ROWS * 2;
    return g_menuKeys;
}

static const char *const *menu_texts(int lang)
{
    if (lang < 0 || lang >= L_COUNT)
        lang = L_EN;
    for (int i = 0; i < MENU_ROWS; i++)
        g_menuTexts[lang][2 * i] = kMenu[i].label[lang], g_menuTexts[lang][2 * i + 1] = kMenu[i].desc[lang];
    return g_menuTexts[lang];
}

/* the game's language: the string table it opens (media\stripped\stringtables\<lang>.zip) */
static void note_language(const WCHAR *path)
{
    const WCHAR *t = path;
    int n;
    for (const WCHAR *p = path; *p; p++)
        if ((*p == '\\' || *p == '/') && CompareStringOrdinal(p + 1, 13, L"stringtables\\", 13, TRUE) == CSTR_EQUAL)
            t = p + 14;
    if (t == path)
        return;
    for (int i = 0; i < LOC_ZIPS; i++) {
        n = lstrlenA(kLocZip[i]);
        WCHAR w[8];
        MultiByteToWideChar(CP_UTF8, 0, kLocZip[i], -1, w, 8);
        if (CompareStringOrdinal(t, n, w, n, TRUE) == CSTR_EQUAL && !lstrcmpiW(t + n, L".zip")) {
            if (InterlockedExchange(&g_lang, kLocLang[i]) != kLocLang[i])
                fh2_say("menu: the game's language: %s", kLocZip[i]);
            return;
        }
    }
}

/* ================================================================================================================ */
/* the page's XAML                                                                                                  */

/* text with `add` put in at `at` (a new buffer) */
static char *str_insert(const char *text, size_t len, size_t at, const char *add, size_t *outLen)
{
    size_t al = strlen(add);
    char *out = (char *)HeapAlloc(GetProcessHeap(), 0, len + al + 1);
    if (!out)
        return NULL;
    memcpy(out, text, at);
    memcpy(out + at, add, al);
    memcpy(out + at + al, text + at, len - at);
    *outLen = len + al;
    out[*outLen] = 0;
    return out;
}

static char *g_xamlAdd; /* a scratch buffer for what goes in */
#define XAML_ADD 32768

static int g_firstRow; /* the list's index of the DualSense's first row */

/* the controller settings page with the DualSense's section, or NULL when it is not as expected */
static char *menu_xaml(const char *text, size_t len, size_t *outLen)
{
    static const char kRes[] = "<ResourceDictionary>";
    static const char kList[] = "x:Name=\"BasicOptionsList\"";
    static const char kAnchor[] = "SelectedIndex=\"{Binding InvertCameraLookBack}\"";
    static const char kWheel[] = "x:Name=\"WheelLayoutPanel\"";
    static const char kPad[] = "x:Name=\"ControllerLayoutPanel\"";
    const char *res = strstr(text, kRes), *list = strstr(text, kList), *anchor = strstr(text, kAnchor);
    const char *wheel = strstr(text, kWheel), *pad = strstr(text, kPad);
    char *a = NULL, *b = NULL, *c = NULL, *d = NULL, *e = NULL;
    size_t la, lb, lc, ld, le, at;
    if (!res || !list || !anchor || !wheel || !pad || !(res < list && list < anchor && anchor < wheel && wheel < pad))
        return NULL;
    if (!g_xamlAdd && !(g_xamlAdd = (char *)HeapAlloc(GetProcessHeap(), 0, XAML_ADD)))
        return NULL;
    /* the rows before ours in the list */
    g_firstRow = 0;
    for (const char *p = list; (p = strstr(p + 1, "<local:SpinnerControl")) && p < anchor;)
        g_firstRow++;
    /* 5. the right side: the game's layout drawings, or our panels (from the end, so the places stay right) */
    {
        const char *grid = wheel, *padEnd = strstr(pad, "</local:FZMaterial>");
        while (grid > text && strncmp(grid, "<Grid>", 6))
            grid--;
        if (grid <= list || !padEnd)
            return NULL;
        padEnd += strlen("</local:FZMaterial>");
        a = str_insert(text, len, (size_t)(padEnd - text),
                       "\r\n                            </Grid>"
                       "\r\n                            <StackPanel Orientation=\"Horizontal\" VerticalAlignment=\"Top\" "
                       "HorizontalAlignment=\"Left\" Visibility=\"{Binding DS_ShowInfo, Converter={StaticResource DS_ShowIf}}\">"
                       "\r\n                                <ContentControl Content=\"{Binding Tag.Description}\" "
                       "DataContext=\"{Binding SelectedItem, ElementName=BasicOptionsList}\" "
                       "Style=\"{StaticResource A_Style_DescriptionPanel}\" Height=\"607\" />"
                       "\r\n                                <DockPanel Width=\"714\" Height=\"607\" Margin=\"6,0,0,0\">"
                       "\r\n                                    <ContentControl DockPanel.Dock=\"Top\" Content=\"DUALSENSE\" "
                       "Style=\"{StaticResource A_Style_DescriptionPanelHeader}\" />"
                       "\r\n                                    <ContentControl DockPanel.Dock=\"Top\" Content=\"{Binding DS_Status}\" "
                       "Template=\"{StaticResource A_ControlTemplate_DescriptionPanelContent}\" />"
                       "\r\n                                </DockPanel>"
                       "\r\n                            </StackPanel>",
                       &la);
        if (!a)
            return NULL;
        b = str_insert(a, la, (size_t)(grid - text) + 6,
                       "\r\n                            <Grid Visibility=\"{Binding DS_ShowInfo, Converter={StaticResource DS_HideIf}}\">", &lb);
        HeapFree(GetProcessHeap(), 0, a);
        if (!b)
            return NULL;
        /* the outer grid keeps the drawings' width, whichever shows: the list does not move */
        c = str_insert(b, lb, (size_t)(grid - text) + 5, " Width=\"1100\"", &lc);
        HeapFree(GetProcessHeap(), 0, b);
        if (!c)
            return NULL;
    }
    /* 4. our rows after the camera look-back switch */
    {
        const char *end = strstr(anchor, "/>");
        char *q = g_xamlAdd;
        if (!end)
            goto bad;
        at = (size_t)(end + 2 - text);
        q[0] = 0;
        for (int i = 0; i < MENU_ROWS; i++) {
            q += strlen(q);
            if (kMenu[i].kind == R_SWITCH)
                _snprintf(q, XAML_ADD - (q - g_xamlAdd),
                          "\r\n                                    <local:SpinnerControl Tag=\"{StaticResource DS_Item_%s}\" "
                          "Style=\"{StaticResource A_Style_SpinnerControl_Default}\" Label=\"{local:Loc MyProfile.IDS_DS_%s}\" "
                          "SelectedIndex=\"{Binding DS_%s}\" ItemsSource=\"{Binding OnOffOptions}\"/>",
                          kMenu[i].key, kMenu[i].key, kMenu[i].key);
            else
                _snprintf(q, XAML_ADD - (q - g_xamlAdd),
                          "\r\n                                    <Slider Tag=\"{StaticResource DS_Item_%s}\" Value=\"{Binding DS_%s}\" "
                          "Style=\"{StaticResource A_Style_Slider_Options}\" Minimum=\"0\" Maximum=\"%d\" SmallChange=\"0.05\" />",
                          kMenu[i].key, kMenu[i].key, (int)kMenu[i].max);
        }
        d = str_insert(c, lc, at, g_xamlAdd, &ld);
        HeapFree(GetProcessHeap(), 0, c), c = NULL;
        if (!d)
            return NULL;
    }
    /* 3. the list's selected row, both ways */
    d[ld] = 0;
    {
        e = str_insert(d, ld, (size_t)(list - text) + strlen(kList), " SelectedIndex=\"{Binding DS_Selected, Mode=TwoWay}\"", &le);
        HeapFree(GetProcessHeap(), 0, d), d = NULL;
        if (!e)
            return NULL;
    }
    /* 2. the resources: the converters and each row's heading and description */
    {
        char *q = g_xamlAdd;
        char *out;
        size_t lo;
        q[0] = 0;
        _snprintf(q, XAML_ADD,
                  "\r\n            <local:FZBooleanToVisibilityConverter x:Key=\"DS_ShowIf\" UseHidden=\"false\" InvertResult=\"false\" />"
                  "\r\n            <local:FZBooleanToVisibilityConverter x:Key=\"DS_HideIf\" UseHidden=\"false\" InvertResult=\"true\" />");
        for (int i = 0; i < MENU_ROWS; i++) {
            q += strlen(q);
            _snprintf(q, XAML_ADD - (q - g_xamlAdd),
                      "\r\n            <local:TileMenuDataItem x:Key=\"DS_Item_%s\" Heading=\"{local:Loc MyProfile.IDS_DS_%s}\" "
                      "Description=\"{local:Loc MyProfile.IDS_DS_%s_Desc}\" />",
                      kMenu[i].key, kMenu[i].key, kMenu[i].key);
        }
        out = str_insert(e, le, (size_t)(res - text) + strlen(kRes), g_xamlAdd, &lo);
        HeapFree(GetProcessHeap(), 0, e);
        if (!out)
            return NULL;
        *outLen = lo;
        return out;
    }
bad:
    if (c)
        HeapFree(GetProcessHeap(), 0, c);
    return NULL;
}

/* ================================================================================================================ */
/* the page's properties                                                                                            */
#define RVA_FLOAT_DESC 0x0ED6E90 /* a float property's descriptor (static, made on the first call) */
#define RVA_BOX_FLOAT 0x03F5990  /* ObjectPtr *(ObjectPtr *out, const float *) */
#define RVA_BOX_INT 0x04702D0    /* ObjectPtr *(ObjectPtr *out, const int *) */
#define RVA_BOX_BOOL 0x0336BA0   /* ObjectPtr *(ObjectPtr *out, const bool *) */
#define RVA_BOOL_TYPE 0x15C9550  /* the bool type, (0) */
#define RVA_STRING_TYPE 0x16A4B30 /* a string property's type (the tabs' badges), (0) */
#define RVA_NOTIFY 0x0CF9C60     /* (object, desc): the property changed */
#define RVA_OBJECT 0x143EB80     /* AVUI::Object::Object(this) */
#define RVA_STRING 0x1663220     /* AVUI String(this, const char *, int length or -1) */

typedef void **(*BoxFn)(void **out, const void *value);
typedef void *(*TypeFn)(int);
typedef void (*NotifyFn)(void *obj, void *desc);
typedef BYTE *(*DescFn)(void);
typedef void (*ObjectFn)(void *self);
typedef void (*StringFn)(void *self, const char *text, int len);
typedef void (*ClassInitFn)(void);

enum { P_ROWS = 0, P_SELECTED = MENU_ROWS, P_SHOWINFO, P_STATUS, P_COUNT };
static const char *g_propName[P_COUNT];
static char g_propNameBuf[MENU_ROWS][40];
static BYTE *g_propDesc[P_COUNT];
static void *g_propNode[P_COUNT][2]; /* {descriptor, next}: the type's list */
static BYTE *g_ctlType;              /* the page's view-model type */
static DescFn g_intDesc;             /* the InvertCameraLookBack descriptor */
static ClassInitFn g_ctlInit;
static volatile LONG g_propsAdded;
static int g_menuHooked;
static volatile LONG g_sel;     /* the list's selected row */
static void *volatile g_selPage; /* the page the selection is of */

#define GAMEFN(rva) ((void *)(g_game + (rva)))

static int menu_value(int row) /* a switch */
{
    const char *k = kMenu[row].key;
    double now = fh2_now();
    if (!strcmp(k, "Triggers"))
        return S.trigOn;
    if (!strcmp(k, "HDHaptics"))
        return S.haptics >= 2;
    if (!strcmp(k, "Gyro"))
        return S.gyro != 0;
    if (!strcmp(k, "RevLights"))
        return S.revLights != 0;
    if (!strcmp(k, "ComboLights"))
        return S.comboLights != 0;
    if (!strcmp(k, "Icons"))
        return S.icons != 0;
    if (!strcmp(k, "TestVibration"))
        return g_test.kind == TEST_VIBRATION && now - g_test.at < 4.5;
    if (!strcmp(k, "TestTriggers"))
        return test_on(TEST_TRIGGERS, now, TEST_TRIGGERS_LEN) || (g_test.kind == TEST_TRIGGERS && now < g_test.at);
    if (!strcmp(k, "TestLights"))
        return test_on(TEST_LIGHTS, now, TEST_LIGHTS_LEN);
    return 0;
}

static float menu_slider(int row)
{
    const char *k = kMenu[row].key;
    if (!strcmp(k, "TriggerStrength"))
        return S.trigStrength;
    if (!strcmp(k, "Vibration"))
        return S.strength;
    if (!strcmp(k, "Engine"))
        return S.carFeel;
    if (!strcmp(k, "Menus"))
        return S.menus;
    if (!strcmp(k, "Events"))
        return S.events;
    if (!strcmp(k, "Surface"))
        return S.surface;
    if (!strcmp(k, "LightBar"))
        return S.brightness;
    return 0;
}

static void test_vibration(void);

/* a switch set in the menu: used at once, kept in the ini */
static void menu_switch(int row, int v)
{
    const char *k = kMenu[row].key;
    double now = fh2_now();
    v = v != 0;
    if (!strcmp(k, "Triggers")) {
        S.trigOn = v;
        fh2_ini_set_int(L"Triggers", L"Enabled", v);
    } else if (!strcmp(k, "HDHaptics")) {
        S.haptics = v ? 3 : 1;
        fh2_ini_set(L"Vibration", L"Mode", v ? L"Auto" : L"Rumble");
        if (!v)
            InterlockedIncrement(&g_hapGen); /* the stream ends: the rumble emulation again */
        else if (g_connected && g_ds.h && !g_ds.bt && !g_hapRunning)
            haptics_start();
    } else if (!strcmp(k, "Gyro")) {
        S.gyro = v ? 1 : 0;
        if (!v)
            T.gyroOn = 0;
        fh2_ini_set(L"Gyro", L"Steering", v ? L"Toggle" : L"Off");
    } else if (!strcmp(k, "RevLights")) {
        S.revLights = v;
        fh2_ini_set_int(L"Lightbar", L"RevLights", v);
    } else if (!strcmp(k, "ComboLights")) {
        S.comboLights = v;
        fh2_ini_set_int(L"Lightbar", L"ComboLights", v);
    } else if (!strcmp(k, "Icons")) {
        S.icons = v ? 2 : 0;
        fh2_ini_set(L"Icons", L"PlayStation", v ? L"Auto" : L"Off");
    } else if (!strcmp(k, "TestVibration")) {
        if (!v)
            return;
        g_test.kind = TEST_VIBRATION, g_test.at = now;
        test_vibration();
    } else if (!strcmp(k, "TestTriggers")) {
        if (!v)
            return;
        g_test.at = now + 0.25, g_test.kind = TEST_TRIGGERS;
    } else if (!strcmp(k, "TestLights")) {
        if (!v)
            return;
        g_test.at = now, g_test.kind = TEST_LIGHTS;
    }
    fh2_say("menu: %s %s", k, v ? "on" : "off");
    InterlockedExchange(&g_outDirty, 1);
    if (g_wake)
        SetEvent(g_wake);
}

static void menu_slide(int row, float v)
{
    const char *k = kMenu[row].key;
    int pct;
    if (!(v == v))
        return;
    if (v < 0)
        v = 0;
    if (v > kMenu[row].max)
        v = kMenu[row].max;
    pct = (int)(v * 100 + 0.5f);
    if (!strcmp(k, "TriggerStrength")) {
        S.trigStrength = v;
        fh2_ini_set_int(L"Triggers", L"Strength", pct);
    } else if (!strcmp(k, "Vibration")) {
        S.strength = v;
        fh2_ini_set_int(L"Vibration", L"Strength", pct);
    } else if (!strcmp(k, "Engine")) {
        S.carFeel = v;
        fh2_ini_set_int(L"Vibration", L"Car", pct);
    } else if (!strcmp(k, "Menus")) {
        S.menus = v;
        fh2_ini_set_int(L"Vibration", L"Menus", pct);
    } else if (!strcmp(k, "Events")) {
        S.events = v;
        fh2_ini_set_int(L"Vibration", L"Events", pct);
    } else if (!strcmp(k, "Surface")) {
        S.surface = v;
        fh2_ini_set_int(L"Vibration", L"Surface", pct);
    } else if (!strcmp(k, "LightBar")) {
        S.brightness = v;
        fh2_ini_set_int(L"Lightbar", L"Brightness", pct);
    }
    InterlockedExchange(&g_outDirty, 1);
    if (g_wake)
        SetEvent(g_wake);
}

/* the controller's state, in the game's language */
static void menu_status(WCHAR *out, int n)
{
    int lang = g_lang >= 0 ? (int)g_lang : (S.menuRu ? L_RU : L_EN), on = g_connected && g_ds.h;
    const char *const *w = kWords[lang];
    char t[1024], *q = t;
    double now = fh2_now();
    t[0] = 0;
    if (!on)
        _snprintf(q, sizeof t, "%s", w[W_NOTCONN]);
    else {
        _snprintf(q, sizeof t, "%s · %s", g_ds.edge ? "DualSense Edge" : "DualSense", g_ds.bt ? "Bluetooth" : "USB");
        q += strlen(q);
        if (T.battery)
            _snprintf(q, sizeof t - (q - t), "\n%s: %d%%%s%s%s", w[W_BATTERY], T.battery, T.charging ? " (" : "",
                      T.charging ? w[W_CHARGING] : "", T.charging ? ")" : "");
        q += strlen(q);
        if (g_ds.update)
            _snprintf(q, sizeof t - (q - t), "\n%s: %04X", w[W_FIRMWARE], g_ds.update);
    }
    q += strlen(q);
    _snprintf(q, sizeof t - (q - t), "\n%s: %s\n%s", w[W_VIBRATION],
              !S.haptics ? w[W_OFF] : g_hapRunning ? w[W_HD] : w[W_RUMBLE], g_abs.on ? w[W_ABS_ON] : w[W_ABS_OFF]);
    q += strlen(q);
    if (g_test.kind == TEST_VIBRATION && now - g_test.at < 4.5)
        _snprintf(q, sizeof t - (q - t), "\n\n%s", w[W_TEST_VIB]);
    else if (g_test.kind == TEST_TRIGGERS && now - g_test.at < TEST_TRIGGERS_LEN)
        _snprintf(q, sizeof t - (q - t), "\n\n%s", w[W_TEST_TRIG]);
    else if (g_test.kind == TEST_LIGHTS && now - g_test.at < TEST_LIGHTS_LEN)
        _snprintf(q, sizeof t - (q - t), "\n\n%s", w[W_TEST_LIGHTS]);
    t[sizeof t - 1] = 0;
    if (!MultiByteToWideChar(CP_UTF8, 0, t, -1, out, n))
        out[0] = 0;
}

/* an AVUI String of the text (the game's constructor widens bytes: it is made from as many 'x' and then given our
   UTF-16 in its buffer, +0x28); the same text's String again, else a new one, never freed (a few a session) */
static BYTE *avui_string(const WCHAR *w)
{
    static struct {
        WCHAR text[600];
        BYTE *s;
    } cache[8];
    static int next;
    char dummy[600];
    int n = lstrlenW(w);
    BYTE *s;
    if (n > 599)
        n = 599;
    for (int i = 0; i < 8; i++)
        if (cache[i].s && !wcsncmp(cache[i].text, w, 600))
            return cache[i].s;
    s = (BYTE *)VirtualAlloc(NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!s)
        return NULL;
    memset(dummy, 'x', n);
    dummy[n] = 0;
    ((StringFn)GAMEFN(RVA_STRING))(s, dummy, n);
    if (*(WCHAR **)(s + 0x28))
        memcpy(*(WCHAR **)(s + 0x28), w, n * sizeof(WCHAR));
    *(LONG *)(s + 0x10) += 1 << 20; /* never freed */
    lstrcpynW(cache[next].text, w, 600);
    cache[next].s = s;
    next = (next + 1) % 8;
    return s;
}

static int prop_of(const BYTE *desc)
{
    for (int i = 0; i < P_COUNT; i++)
        if (g_propDesc[i] == desc)
            return i;
    return -1;
}

static void notify(void *page, int prop)
{
    if (page && g_propDesc[prop])
        ((NotifyFn)GAMEFN(RVA_NOTIFY))(page, g_propDesc[prop]);
}

static void **prop_get(void **out, BYTE *desc, void *page)
{
    int p = prop_of(desc);
    *out = NULL;
    if (p < 0)
        return out;
    if (p < P_SELECTED && kMenu[p].kind == R_SLIDER) {
        float f = menu_slider(p);
        ((BoxFn)GAMEFN(RVA_BOX_FLOAT))(out, &f);
    } else if (p < P_SELECTED) {
        int v = menu_value(p);
        ((BoxFn)GAMEFN(RVA_BOX_INT))(out, &v);
    } else if (p == P_SELECTED) {
        int v;
        if (page && page != g_selPage) /* a new page: from its first row */
            g_selPage = page, g_sel = 0;
        v = g_sel;
        ((BoxFn)GAMEFN(RVA_BOX_INT))(out, &v);
    } else if (p == P_SHOWINFO) {
        BYTE b = g_menuOn && g_sel >= g_firstRow && g_sel < g_firstRow + MENU_ROWS;
        ((BoxFn)GAMEFN(RVA_BOX_BOOL))(out, &b);
    } else if (p == P_STATUS) {
        WCHAR w[600];
        BYTE *s;
        menu_status(w, 600);
        s = avui_string(w);
        if (s) {
            InterlockedIncrement((LONG *)(s + 0x10)); /* the caller releases it */
            *out = s;
        }
    }
    return out;
}

static void prop_set(BYTE *desc, void *page, BYTE *value)
{
    int p = prop_of(desc);
    if (p < 0 || !value)
        return;
    if (p < P_SELECTED && kMenu[p].kind == R_SLIDER)
        menu_slide(p, *(float *)(value + 0x28));
    else if (p < P_SELECTED) {
        int v = *(int *)(value + 0x28);
        if (v != menu_value(p))
            menu_switch(p, v);
    } else if (p == P_SELECTED) {
        int v = *(int *)(value + 0x28);
        if (page)
            g_selPage = page;
        if (v == g_sel)
            return;
        g_sel = v;
        /* the right side, the state and the tests' switches follow the selection */
        notify(page, P_SHOWINFO);
        notify(page, P_STATUS);
        for (int i = ROW_TEST_VIBRATION; i < MENU_ROWS; i++)
            notify(page, i);
    }
}

/* a descriptor laid out as the model's (its vtable, its +0x38, its type unless given, its flags) with our getter,
   setter and name */
static BYTE *new_desc(const BYTE *model, const char *name, void *type)
{
    BYTE *d;
    if (!model)
        return NULL;
    d = (BYTE *)VirtualAlloc(NULL, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!d)
        return NULL;
    ((ObjectFn)GAMEFN(RVA_OBJECT))(d);
    memcpy(d, model, 8);
    *(LONG *)(d + 0x10) += 1 << 20; /* never freed */
    *(void **)(d + 0x28) = (void *)prop_get;
    *(void **)(d + 0x30) = (void *)prop_set;
    memcpy(d + 0x38, model + 0x38, 8);
    *(const char **)(d + 0x40) = name;
    if (type)
        *(void **)(d + 0x48) = type;
    else
        memcpy(d + 0x48, model + 0x48, 8);
    d[0x50] = model[0x50];
    return d;
}

/* ours after the page's own, once */
static void menu_add_props(void)
{
    BYTE *intModel, *floatModel;
    int made = 0;
    if (InterlockedExchange(&g_propsAdded, 1))
        return;
    intModel = g_intDesc(), floatModel = ((DescFn)GAMEFN(RVA_FLOAT_DESC))();
    for (int i = 0; i < P_COUNT; i++) {
        if (i < P_SELECTED)
            g_propDesc[i] = new_desc(kMenu[i].kind == R_SLIDER ? floatModel : intModel, g_propName[i], NULL);
        else if (i == P_SELECTED)
            g_propDesc[i] = new_desc(intModel, g_propName[i], NULL);
        else if (i == P_SHOWINFO)
            g_propDesc[i] = new_desc(intModel, g_propName[i], ((TypeFn)GAMEFN(RVA_BOOL_TYPE))(0));
        else
            g_propDesc[i] = new_desc(intModel, g_propName[i], ((TypeFn)GAMEFN(RVA_STRING_TYPE))(0));
        if (!g_propDesc[i])
            continue;
        g_propNode[i][0] = g_propDesc[i];
        g_propNode[i][1] = *(void **)(g_ctlType + 0x40);
        *(void **)(g_ctlType + 0x40) = g_propNode[i];
        made++;
    }
    fh2_say("menu: %d of %d properties added to the controller settings page", made, P_COUNT);
}

static void hk_ctl_init(void)
{
    g_ctlInit();
    menu_add_props();
}

/* the game's code the menu needs, as it is in FH2 1.0.0.64: each function's first bytes */
static int menu_code_ok(void)
{
    static const struct {
        DWORD rva;
        BYTE b[8];
    } k[] = {{RVA_FLOAT_DESC, {0x40, 0x57, 0x48, 0x83, 0xEC, 0x30, 0x48, 0xC7}},
             {RVA_BOX_FLOAT, {0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B}},
             {RVA_BOX_INT, {0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x48, 0x8B}},
             {RVA_BOX_BOOL, {0x40, 0x53, 0x48, 0x83, 0xEC, 0x30, 0x80, 0x3A}},
             {RVA_BOOL_TYPE, {0x80, 0x3D, 0x0D, 0xB8, 0x0E, 0x02, 0x00, 0x75}},
             {RVA_STRING_TYPE, {0x48, 0x8D, 0x05, 0x59, 0xB0, 0x02, 0x02, 0xC3}},
             {RVA_NOTIFY, {0x40, 0x57, 0x48, 0x83, 0xEC, 0x60, 0x48, 0xC7}},
             {RVA_OBJECT, {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83}},
             {RVA_STRING, {0x48, 0x89, 0x4C, 0x24, 0x08, 0x57, 0x48, 0x83}}};
    for (int i = 0; i < (int)(sizeof k / sizeof k[0]); i++)
        if (memcmp(g_game + k[i].rva, k[i].b, 8)) {
            fh2_say("menu: the game's code at game+0x%lX is not as expected", k[i].rva);
            return 0;
        }
    return 1;
}

/* at the game's code: the page type's property list function hooked (found as 1.x's ui.h found it) */
static int menu_install(BYTE *base)
{
    static const char kName[] = "InvertCameraLookBack";
    static const BYTE kProlog[18] = {0x40, 0x53, 0x48, 0x81, 0xEC, 0xD0, 0x00, 0x00, 0x00,
                                     0x48, 0xC7, 0x44, 0x24, 0x20, 0xFE, 0xFF, 0xFF, 0xFF};
    Image g;
    BYTE *str = NULL, *L = NULL, *descFn, *C = NULL, *classInit;
    if (!g_menuOn) {
        fh2_say("menu: the page is the game's own this time (files.h) -- no properties");
        return 0;
    }
    for (int i = 0; i < MENU_ROWS; i++) {
        wsprintfA(g_propNameBuf[i], "DS_%s", kMenu[i].key);
        g_propName[i] = g_propNameBuf[i];
    }
    g_propName[P_SELECTED] = "DS_Selected", g_propName[P_SHOWINFO] = "DS_ShowInfo", g_propName[P_STATUS] = "DS_Status";
    if (!image_of(base, &g) || !menu_code_ok()) {
        ds_note("menu: the game's code is not as expected -- the DualSense's rows show nothing");
        return 0;
    }
    for (BYTE *p = g.rdata + 1; p + sizeof kName <= g.rdataEnd; p++)
        if (*p == 'I' && p[-1] == 0 && !memcmp(p, kName, sizeof kName)) {
            str = p;
            break;
        }
    /* the descriptor's function: lea rax,[name] with its known neighbours */
    for (BYTE *p = g.text; str && p + 7 <= g.textEnd && !L; p++)
        if (p[0] == 0x48 && p[1] == 0x8D && p[2] == 0x05 && rel32(p, 3, 7) == str && p - 0x40 >= g.text &&
            p[-0x40] == 0x33 && p[-0x3F] == 0xC9 && p[-0x3E] == 0xE8 && !memcmp(p - 0x36, "\x48\x8D\x3D", 3) &&
            p[-0x2C] == 0xE8 && !memcmp(p - 0x27, "\x48\x8D\x05", 3) && !memcmp(p - 0x19, "\x48\x8D\x05", 3) &&
            !memcmp(p + 0x1C, "\x48\x8D\x05", 3))
            L = p;
    descFn = L ? func_start(g.base, L) : NULL;
    /* the type's list: the call of descFn, then AddProperty(type, &pair) */
    for (BYTE *p = g.text; descFn && p + 0x26 <= g.textEnd && !C; p++)
        if (p[0] == 0xE8 && rel32(p, 1, 5) == descFn && !memcmp(p + 5, "\x48\x89\x05", 3) &&
            !memcmp(p + 0x13, "\x48\x8D\x15", 3) && !memcmp(p + 0x1A, "\x48\x8D\x0D", 3) && p[0x21] == 0xE8)
            C = p;
    classInit = C ? func_start(g.base, C) : NULL;
    if (!classInit || memcmp(classInit, kProlog, sizeof kProlog)) {
        ds_note("menu: the controller settings' code is not as expected (%p %p) -- the DualSense's rows show nothing",
                (void *)descFn, (void *)classInit);
        return 0;
    }
    g_intDesc = (DescFn)descFn;
    g_ctlType = rel32(C + 0x1A, 3, 7);
    if (!mh_hook("controller settings: its properties", classInit, (void *)hk_ctl_init, (void **)&g_ctlInit)) {
        ds_note("menu: the controller settings' code could not be hooked -- the DualSense's rows show nothing");
        return 0;
    }
    g_menuHooked = 1;
    fh2_say("menu: the controller settings' properties hooked (game+0x%lX, its type game+0x%lX)", (DWORD)(classInit - g.base),
            (DWORD)(g_ctlType - g.base));
    if (*(void **)(g_ctlType + 0x40)) /* its list was made already: ours now */
        menu_add_props();
    return 1;
}

/* the vibration test: its voices, from 0.25 s on (after the menu's own click) */
static void test_vibration(void)
{
    const float t = 0.25f;
    fx_add(FX_THUMP, t, 0.8f, 50, 0, 0, 0.12f, -1, 0);
    fx_add(FX_THUMP, t + 0.5f, 0.8f, 50, 0, 0, 0.12f, 1, 0);
    for (int i = 0; i < 5; i++)
        fx_add(FX_TICK, t + 1.0f + 0.12f * i, 0.6f, 235, 0, 0, 0.0035f, -1 + 0.5f * i, 0);
    fx_add(FX_TICK, t + 1.8f, 0.8f, 170, 0, 0, 0.006f, 0, 0);
    fx_add(FX_THUMP, t + 1.8f, 0.4f, 75, 0, 0, 0.014f, 0, 0);
    fx_add(FX_RATCHET, t + 2.2f, 0.3f, 230, 22, 0.8f, 0.003f, 0, 0);
    fx_add(FX_SWEEP, t + 3.2f, 0.35f, 80, 240, 0.25f, 0, 0, 0);
    fx_add(FX_THUMP, t + 3.6f, 0.95f, 42, 0, 0, 0.11f, 0, 0);
    fx_add(FX_NOISE, t + 3.6f, 0.4f, 240, 0, 0, 0.03f, 0, 0);
}
