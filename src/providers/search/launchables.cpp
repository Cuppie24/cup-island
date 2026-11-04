#include "providers/search/launchables.h"

#include "indexer/query.h"

#include <algorithm>
#include <cstdlib>
#include <set>
#include <sstream>

namespace cup {
namespace {

bool startsWith(std::string_view text, std::string_view prefix) { return text.substr(0, prefix.size()) == prefix; }
bool endsWith(std::string_view text, std::string_view suffix)
{
    return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

// UTF-8 characters, as the island counts a name's length.
std::size_t letters(std::string_view text)
{
    std::size_t n = 0;
    for (char c : text) n += (static_cast<unsigned char>(c) & 0xC0) != 0x80;
    return n;
}

bool separator(char c) { return c == ' ' || c == '-' || c == '_' || c == '.' || c == '(' || c == ')' || c == '&' || c == '+' || c == ','; }

// A letter's case, for "VisualStudio" and "ВКонтакте": ASCII, and Russian's
// А-Я (D0 90..AF) and а-я (D0 B0..BF, D1 80..8F), the two scripts search folds.
bool capitalAt(std::string_view s, std::size_t i)
{
    auto b = static_cast<unsigned char>(s[i]);
    if (b >= 'A' && b <= 'Z') return true;
    return b == 0xD0 && i + 1 < s.size() && static_cast<unsigned char>(s[i + 1]) >= 0x90 && static_cast<unsigned char>(s[i + 1]) <= 0xAF;
}
bool smallAt(std::string_view s, std::size_t i)
{
    auto b = static_cast<unsigned char>(s[i]);
    if (b >= 'a' && b <= 'z') return true;
    if (i + 1 >= s.size()) return false;
    auto n = static_cast<unsigned char>(s[i + 1]);
    return (b == 0xD0 && n >= 0xB0 && n <= 0xBF) || (b == 0xD1 && n >= 0x80 && n <= 0x8F);
}

// The first letter of each word, folded: "Visual Studio Code" -> "vsc",
// "ВКонтакте" -> "вк" (a capital after a small letter starts a word too).
std::string initials(std::string_view name)
{
    std::string out;
    std::size_t previous = std::string_view::npos; // the last character's first byte
    for (std::size_t i = 0; i < name.size();) {
        std::size_t length = 1;
        auto b = static_cast<unsigned char>(name[i]);
        if (b >= 0xF0) length = 4;
        else if (b >= 0xE0) length = 3;
        else if (b >= 0xC0) length = 2;
        length = std::min(length, name.size() - i);
        bool starts = !separator(name[i]) &&
                      (previous == std::string_view::npos || separator(name[previous]) || (capitalAt(name, i) && smallAt(name, previous)));
        if (starts) out += indexer::foldCase(name.substr(i, length));
        previous = i;
        i += length;
    }
    return out;
}

struct Page {
    const char* link;
    const char* english;
    const char* russian;
    const char* words; // more to match, space-separated, any case
    int minBuild = 0;  // 0: every supported build (decision 015's 17763 on)
};

// The pages people look for (decision 077). Windows 10 1809 is build 17763,
// Windows 11 starts at 22000.
const Page kPages[] = {
    { "ms-settings:display", "Display", "Дисплей", "screen brightness resolution monitor scale экран яркость разрешение монитор масштаб" },
    { "ms-settings:nightlight", "Night light", "Ночной свет", "night blue light ночной синий" },
    { "ms-settings:sound", "Sound", "Звук", "audio volume speakers microphone output input громкость динамики микрофон" },
    { "ms-settings:apps-volume", "Volume mixer", "Микшер громкости", "sound mixer звук микшер", 22000 },
    { "ms-settings:notifications", "Notifications", "Уведомления", "alerts toasts оповещения" },
    { "ms-settings:quiethours", "Focus assist", "Фокусировка внимания", "do not disturb focus не беспокоить фокусировка" },
    { "ms-settings:powersleep", "Power & sleep", "Питание и спящий режим", "power sleep screen off питание сон" },
    { "ms-settings:batterysaver", "Battery saver", "Экономия заряда", "battery energy батарея аккумулятор" },
    { "ms-settings:storagesense", "Storage", "Память устройства", "disk space storage cleanup диск место хранилище очистка" },
    { "ms-settings:multitasking", "Multitasking", "Многозадачность", "snap windows virtual desktops окна рабочие столы" },
    { "ms-settings:clipboard", "Clipboard", "Буфер обмена", "clipboard history copy paste буфер история" },
    { "ms-settings:about", "About", "О системе", "system info pc name version specs о компьютере версия характеристики" },
    { "ms-settings:bluetooth", "Bluetooth & devices", "Bluetooth и устройства", "bluetooth devices pair блютуз устройства" },
    { "ms-settings:printers", "Printers & scanners", "Принтеры и сканеры", "printer scanner print принтер сканер печать" },
    { "ms-settings:mousetouchpad", "Mouse", "Мышь", "mouse cursor pointer scroll мышь курсор прокрутка" },
    { "ms-settings:devices-touchpad", "Touchpad", "Сенсорная панель", "touchpad gestures тачпад жесты" },
    { "ms-settings:typing", "Typing", "Ввод", "keyboard autocorrect spelling клавиатура автозамена" },
    { "ms-settings:autoplay", "AutoPlay", "Автозапуск", "autoplay usb drive автозапуск" },
    { "ms-settings:network-status", "Network & internet", "Сеть и Интернет", "network internet connection сеть интернет подключение" },
    { "ms-settings:network-wifi", "Wi-Fi", "Wi-Fi", "wifi wireless wlan вайфай беспроводная интернет" },
    { "ms-settings:network-ethernet", "Ethernet", "Ethernet", "cable lan wired кабель проводная" },
    { "ms-settings:network-vpn", "VPN", "VPN", "vpn впн" },
    { "ms-settings:network-proxy", "Proxy", "Прокси-сервер", "proxy прокси" },
    { "ms-settings:network-airplanemode", "Airplane mode", "Режим «в самолете»", "airplane flight режим самолет" },
    { "ms-settings:network-mobilehotspot", "Mobile hotspot", "Мобильный хот-спот", "hotspot tethering share internet точка доступа раздать" },
    { "ms-settings:personalization-background", "Background", "Фон", "wallpaper desktop background обои фон рабочего стола" },
    { "ms-settings:colors", "Colors", "Цвета", "dark mode light mode accent theme тёмная темная светлая тема акцент" },
    { "ms-settings:lockscreen", "Lock screen", "Экран блокировки", "lock screen экран блокировки" },
    { "ms-settings:themes", "Themes", "Темы", "theme cursor icons тема значки" },
    { "ms-settings:fonts", "Fonts", "Шрифты", "font typeface шрифт" },
    { "ms-settings:taskbar", "Taskbar", "Панель задач", "taskbar tray панель задач трей" },
    { "ms-settings:personalization-start", "Start", "Пуск", "start menu меню пуск" },
    { "ms-settings:appsfeatures", "Installed apps", "Установленные приложения", "uninstall programs apps features удалить программы приложения" },
    { "ms-settings:defaultapps", "Default apps", "Приложения по умолчанию", "default browser open with по умолчанию браузер" },
    { "ms-settings:startupapps", "Startup apps", "Автозагрузка", "startup autostart boot автозагрузка запуск" },
    { "ms-settings:optionalfeatures", "Optional features", "Дополнительные компоненты", "features components компоненты" },
    { "ms-settings:yourinfo", "Your info", "Ваши данные", "account profile picture аккаунт профиль" },
    { "ms-settings:emailandaccounts", "Email & accounts", "Электронная почта и учетные записи", "email mail accounts почта учетные записи" },
    { "ms-settings:signinoptions", "Sign-in options", "Варианты входа", "password pin windows hello fingerprint пароль пин отпечаток" },
    { "ms-settings:otherusers", "Other users", "Другие пользователи", "users family accounts пользователи семья" },
    { "ms-settings:dateandtime", "Date & time", "Дата и время", "clock time zone date часы часовой пояс дата" },
    { "ms-settings:regionlanguage", "Language & region", "Язык и регион", "language keyboard layout region язык раскладка регион" },
    { "ms-settings:gaming-gamebar", "Game Bar", "Xbox Game Bar", "xbox game bar record игровая панель запись" },
    { "ms-settings:easeofaccess-display", "Accessibility", "Специальные возможности", "accessibility text size ease of access специальные возможности размер текста" },
    { "ms-settings:privacy", "Privacy & security", "Конфиденциальность и защита", "privacy permissions конфиденциальность разрешения" },
    { "ms-settings:privacy-location", "Location", "Расположение", "location gps геолокация местоположение" },
    { "ms-settings:privacy-webcam", "Camera", "Камера", "camera webcam камера вебкамера" },
    { "ms-settings:privacy-microphone", "Microphone", "Микрофон", "microphone mic микрофон" },
    { "ms-settings:windowsupdate", "Windows Update", "Центр обновления Windows", "update updates upgrade обновление обновления" },
    { "ms-settings:windowsdefender", "Windows Security", "Безопасность Windows", "antivirus defender virus firewall антивирус защитник вирус брандмауэр" },
    { "ms-settings:backup", "Backup", "Резервное копирование", "backup onedrive резервная копия" },
    { "ms-settings:recovery", "Recovery", "Восстановление", "reset recovery factory restore сброс восстановление" },
    { "ms-settings:activation", "Activation", "Активация", "activate license product key активация лицензия ключ" },
    { "ms-settings:developers", "For developers", "Для разработчиков", "developer mode режим разработчика" },
};

std::vector<std::string> wordsOf(std::string_view text)
{
    std::vector<std::string> out;
    std::istringstream in{ indexer::foldCase(text) };
    for (std::string w; in >> w;) out.push_back(w);
    return out;
}

} // namespace

std::vector<Launchable> cleanedApps(std::vector<Launchable> apps)
{
    std::vector<Launchable> out;
    std::set<std::string> seen;
    for (Launchable& a : apps) {
        std::string name = indexer::foldCase(a.name);
        std::string target = indexer::foldCase(a.target);
        bool uninstaller = startsWith(name, "uninstall") || startsWith(name, "удалить") || startsWith(name, "удаление");
        bool document = false;
        for (const char* ext : { ".chm", ".url", ".htm", ".html", ".pdf", ".txt", ".rtf", ".md", ".hlp" })
            document = document || endsWith(target, ext);
        bool web = startsWith(target, "http://") || startsWith(target, "https://");
        if (a.name.empty() || a.target.empty() || uninstaller || document || web || !seen.insert(target).second) continue;
        out.push_back(std::move(a));
    }
    return out;
}

std::vector<Launchable> settingsPages(int windowsBuild, bool russian)
{
    std::vector<Launchable> out;
    for (const Page& p : kPages) {
        if (p.minBuild && windowsBuild < p.minBuild) continue;
        Launchable page{ Launchable::Kind::Setting, russian ? p.russian : p.english, p.link, wordsOf(p.words), nullptr };
        for (std::string& w : wordsOf(russian ? p.english : p.russian)) page.words.push_back(std::move(w)); // the other name matches too
        out.push_back(std::move(page));
    }
    return out;
}

void LaunchHistory::record(const std::string& target, Ms now)
{
    Use& use = uses[target];
    ++use.count;
    use.last = now;
    while (uses.size() > kMaxHistory) {
        auto oldest = std::min_element(uses.begin(), uses.end(), [](const auto& a, const auto& b) { return a.second.last < b.second.last; });
        uses.erase(oldest);
    }
}

std::string LaunchHistory::save() const
{
    std::string out;
    for (const auto& [target, use] : uses) out += std::to_string(use.count) + " " + std::to_string(use.last.count()) + " " + target + "\n";
    return out;
}

LaunchHistory LaunchHistory::load(std::string_view text)
{
    LaunchHistory h;
    std::istringstream in{ std::string(text) };
    for (std::string line; std::getline(in, line);) {
        std::istringstream fields(line);
        int count = 0;
        long long last = 0;
        std::string target;
        if (!(fields >> count >> last) || count <= 0) continue;
        std::getline(fields >> std::ws, target);
        if (!target.empty()) h.uses[target] = { count, Ms{ last } };
    }
    return h;
}

double LaunchHistory::weight(const std::string& target, Ms now) const
{
    constexpr double kMonthMs = 30.0 * 24 * 60 * 60 * 1000;
    auto found = uses.find(target);
    if (found == uses.end()) return 0;
    double age = static_cast<double>((now - found->second.last).count());
    if (age >= kMonthMs) return 0;
    return found->second.count * (1.0 - std::max(age, 0.0) / kMonthMs);
}

int launchScore(const Launchable& item, std::string_view query)
{
    indexer::Query q = indexer::parseQuery(query);
    if (q.words.empty()) return 0;
    std::string firsts = initials(item.name);
    int total = 0;
    for (const std::string& w : q.words) {
        int best = indexer::matchScore(item.name, w);
        if (w.size() >= 2 && startsWith(firsts, w)) best = std::max(best, 2);
        for (const std::string& word : item.words) best = std::max(best, indexer::matchScore(word, w));
        if (best == 0) return 0;
        total += best;
    }
    return total;
}

std::vector<std::size_t> rankLaunchables(const std::vector<Launchable>& items, std::string_view query, const LaunchHistory& history,
                                          Ms now, std::size_t limit)
{
    struct Scored {
        std::size_t index;
        int score;
        double used;
    };
    std::vector<Scored> found;
    indexer::Query q = indexer::parseQuery(query);
    // Something typed but no plain words (a path "d:/projects", ext:pdf,
    // folder:): it's for files; an app or Settings page matches none of it.
    // Only nothing typed at all lists what was used before.
    if (q.words.empty() && !q.empty()) return {};
    bool anyWords = !q.words.empty();
    for (std::size_t i = 0; i < items.size(); ++i) {
        double used = history.weight(items[i].target, now);
        int score = anyWords ? launchScore(items[i], query) : 0;
        if ((anyWords && score > 0) || (!anyWords && used > 0)) found.push_back({ i, score, used });
    }
    std::sort(found.begin(), found.end(), [&](const Scored& a, const Scored& b) {
        if (a.score != b.score) return a.score > b.score;
        if (a.used != b.used) return a.used > b.used;
        std::size_t la = letters(items[a.index].name), lb = letters(items[b.index].name);
        if (la != lb) return la < lb;
        return items[a.index].name < items[b.index].name;
    });
    std::vector<std::size_t> out;
    for (std::size_t i = 0; i < found.size() && i < limit; ++i) out.push_back(found[i].index);
    return out;
}

} // namespace cup
