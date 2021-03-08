#include <iostream>
#include <regex>
#include <filesystem>
#include <string>
#include <sstream>

namespace fs = std::filesystem;
using namespace std::string_literals;
namespace fileop
{
    fs::path expand_env(const std::string &text, const bool &verbose)
    {
        static const std::regex env_re{R"--(\$\{([^}]+)\})--"}; //regex to match ${...}
        std::smatch match;
        std::stringstream result;
        for (auto it = std::sregex_iterator(text.begin(), text.end(), env_re); it != std::sregex_iterator(); it++) // iterate through all matches
        {
            match = *it;
            result << match.prefix(); // add the prefix (i.e add chars between matches or beginning) to result
            if (verbose)
                std::cout << match.str() << "\n";
            auto const from = match[0];           //entire match
            auto const var_name = match[1].str(); // captured group i.e name of env variable
            const char *env = std::getenv(var_name.c_str());
            if (env == nullptr) // varname NOT a valid environement variable
            {
                char input;
                while (true)
                {
                    std::cout << "The Environment variable " << var_name << " doesn't exist.\n"
                              << "1) [A]bort\n"
                              << "2) Enter [N]ew value\n"
                              << "3) [C]ontinue with '" << from.str() << "'\n"
                              << "Enter option (A/N/C): ";
                    input = std::cin.get();
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    input = toupper(input);

                    if (input == 'A' || input == 'N' || input == 'C')
                        break;
                    std::cin.clear();
                    std::cout << "You may only type 'A' or 'N' or 'C'.\n";
                }

                switch (input)
                {
                case 'N': // Provide Replacement value
                {
                    std::string new_path;
                    std::cout << "Enter New Path: ";
                    std::getline(std::cin, new_path);
                    result << new_path;
                }
                break;
                case 'C': // continue with ${...}
                    result << match.str();
                    break;
                case 'A':
                default: // Abort
                    std::cout << "Aborting... \n";
                    std::exit(1);
                    break;
                }
            }
            else
                result << std::string(env); // append envrionment variable's value to the path string
        }
        result << match.suffix();
        return fs::path((!result.str().empty()) ? result.str() : text).lexically_normal();
    }

    std::tuple<fs::path, std::string, std::string> get_path_and_args(const fs::path filepath, const bool &print_filepath)
    {
        fs::path media_dir;  // parent directory of media file
        std::string recode;  // recode to different format
        std::string quality; // quality of video or audio (bestvideo+bestuadio / bestaudio)
        char *media_env_var; // env-var $MUSIC or $VIDEOS depending on extension
        std::string filename = filepath.filename().string();
        std::string extension_string = filepath.extension().string().substr(1);
        if (std::regex_match(filename, std::regex(".+\\.(mp3|flac|aac|m4a|opus|vorbis|wav)$")))
        // is audio file
        {
            media_env_var = std::getenv("MUSIC");
            recode = "-x --audio-format "s + extension_string;
            quality = "bestaudio";
        }
        else if (std::regex_match(filename, std::regex(".+\\.(mp4|flv|ogg|webm|mkv|avi)$")))
        // is video file
        {
            media_env_var = std::getenv("VIDEOS");
            recode = "--recode-video "s + extension_string;
            quality = "bestvideo+bestaudio";
        }
        else
        // is neither audio or video! abort and report to stderr!
        {
            std::cerr << "Invalid file format! " << extension_string << "\n";
            std::exit(1);
        }

        if (filepath.has_parent_path())
        {
            fs::path parentPath = filepath.parent_path();
            if (!fs::exists(parentPath)) // create parent directories if they don't exist
            {
                std::cout << "Creating parent directories... " << parentPath.lexically_normal().string() << "\n";
                fs::create_directories(parentPath);
            }
            media_dir = parentPath;
        }
        else
            // check if preferred parent is set and exists, else set parent to current path
            media_dir = (media_env_var == nullptr || !fs::exists(media_env_var)) ? fs::current_path() : fs::path(media_env_var);

        // media_dir is now filepath.

        if (print_filepath)
            std::cout << (media_dir / filename).string() << "\n";

        media_dir /= filepath.stem(); // append filename to media directory to form filepath
        return {media_dir.make_preferred().lexically_normal(), recode, quality};
    }

    void check_has_extension(const fs::path &filepath)
    {
        // extension-less file! terminate and report to stderr
        if (filepath.extension().empty())
        {
            std::cerr << "Invalid Filename! " << filepath << '\n';
            std::exit(1);
        }
    }

} // namespace fileop

fs::path get_filepath(const bool &verbose)
{
    std::string filepath;
    std::cout << "Enter filepath: ";
    std::getline(std::cin, filepath); // input can include spaces so getline is used
    filepath = fileop::expand_env(filepath, verbose);
    return filepath;
}

std::string get_URL()
{
    std::string URL;
    std::cout << "Enter URL: ";
    std::getline(std::cin, URL); // input can include spaces so getline is used
    return URL;
}

int main(int argc, char **argv)
{
    bool verbose = false;
    bool print_filename = true;
    bool disable_mtime = false;
    int non_flag_args = 0;
    fs::path filepath;
    bool flag_lock = false;
    std::string URL;
    std::vector<std::string> arguments;
    for (char **arg = argv + 1; *arg; arg++)
    {
        std::string arg_str = std::string(*arg);
        if (arg_str[0] != '-' && !flag_lock)
        {
            if (non_flag_args == 0)
                filepath = fs::path(arg_str);
            else if (non_flag_args == 1)
                URL = arg_str;
            non_flag_args++;
        }
        else
        {
            if (arg_str == "--noprint")
                print_filename = false;
            else if (arg_str == "--utime")
                disable_mtime = true;
            else
            {
                if (arg_str == "-v")
                    verbose = true;
                arguments.push_back(arg_str);
                flag_lock = true;
            }
        }
    }
    if (non_flag_args < 1)
        filepath = get_filepath(verbose);

    fileop::check_has_extension(filepath);
    auto [filepath_noext, recode, quality] = fileop::get_path_and_args(filepath, print_filename);
    filepath = fs::absolute(filepath_noext); // resolve and convert to absolute

    if (non_flag_args < 2)
        URL = get_URL();

    if (!std::regex_match(URL, std::regex(R"(^(https?\:\/\/)?(www\.youtube\.com|youtu\.?be)\/.+$)")))
    {
        char input;
        do
        {
            std::cout << "'" << URL << "' is not a valid URL."
                      << " Search \"" << URL
                      << "\" on youtube instead?(y/n): ";
            std::cin.get(input);
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

            if ((input == 'y') || (input == 'n'))
                break;
            std::cout << "entered: '" << input << "'\n";
            std::cout << "You may only type 'y' or 'n'.\n";
        } while (true);

        if (input == 'y')
            URL = "'ytsearch:" + URL + "'"; // change URL(i.e search_term) to 'ytsearch:URL'
        else
            std::exit(1);
    }

    std::string command = "youtube-dl -f "s + quality + " "s + URL + " "s + recode + " -o '" + filepath.string() + ".%(ext)s'";
    if (!disable_mtime)
        command += " --no-mtime "s;
    for (std::string i : arguments)
        command += " "s + i;

    if (verbose)
        std::cout << command << "\n";
    std::system(command.c_str());
}
