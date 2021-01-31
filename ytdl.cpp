#include <iostream>
#include <regex>
#include <filesystem>
#include <string>
#include <sstream>
namespace fs = std::filesystem;
namespace fileop
{
    fs::path expand_env(const std::string &text, const bool &verbose)
    {
        static const std::regex env_re{R"--(\$\{([^}]+)\})--"};
        std::smatch match;
        std::stringstream result;
        for (auto i = std::sregex_iterator(text.begin(), text.end(), env_re); i != std::sregex_iterator(); i++)
        {
            match = *i;
            result << match.prefix();
            if (verbose)
                std::cout << match.str() << "\n";
            auto const from = match[0];
            auto const var_name = match[1].str();
            const char *env = std::getenv(var_name.c_str());
            if (env == nullptr)
            {
                char input;
                do
                {
                    std::cout << "The Environment variable " << var_name << " doesn't exist.\n"
                              << "1) Abort(A)\n"
                              << "2) Enter new value(N)\n"
                              << "3) Continue with '" << from.str() << "' (C)\n"
                              << "Enter option (A/N/C): ";
                    std::cin >> input;
                    char input = toupper(input);
                    if ((input == 'A') || (input == 'N') || (input = 'C'))
                        break;
                    std::cout << "entered: '" << input << "'\n";
                    std::cout << "You may only type 'A' or 'N' or 'C'.\n";
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                } while (true);
                switch (input)
                {
                case 'A':
                    std::cout << "Aborting...\n";
                    std::exit(1);
                    break;
                case 'N':
                {
                    std::string new_path;
                    std::cout << "Enter New Path: ";
                    std::cin >> new_path;
                    result << new_path;
                }
                break;
                case 'C':
                    result << match.str();
                    break;
                }
            }
            else
                result << std::string(env);
        }
        result << match.suffix();
        return fs::path((!result.str().empty()) ? result.str() : text).lexically_normal();
    }
    fs::path resolve_filepath(const fs::path &filepath, const char *preferred_parent)
    {
        fs::path parentDir;
        if (filepath.has_parent_path())
        {
            fs::path parentPath = filepath.parent_path();
            if (!fs::exists(parentPath)) // create parent directories if they don't exist
            {
                std::cout << "Creating parent directories..." << parentPath.lexically_normal().string() << "\n";
                fs::create_directories(parentPath);
            }
            parentDir = parentPath;
        }
        else
            // check if preferred parent is set and exists, else set parent to current path
            parentDir = (preferred_parent == nullptr || !fs::exists(preferred_parent)) ? fs::current_path() : fs::path(preferred_parent);
        return parentDir / filepath.stem();
    }
} // namespace fileop

fs::path get_filepath(const bool &verbose)
{
    std::string filepath;
    std::cout << "Enter filepath: ";
    std::cin >> filepath;
    filepath = fileop::expand_env(filepath, verbose);
    return filepath;
}

std::string get_URL()
{
    std::string URL;
    std::cout << "Enter URL: ";
    std::cin.ignore();
    std::getline(std::cin, URL);
    return URL;
}

using namespace std::string_literals;
int main(int argc, const char **argv)
{
    bool verbose = (std::string(argv[argc - 1]) == "-v");
    fs::path filepath;
    std::string URL;
    filepath = (argc < 2 || (verbose && argc < 3)) ? get_filepath(verbose) : argv[1];

    // extensionless file! terminate and report to stderr
    if (filepath.extension().empty())
    {
        std::cerr << "Invalid Filename! " << filepath.string() << '\n';
        std::exit(1);
    }

    std::string extension = filepath.extension().string().substr(1);
    fs::path filename = filepath.filename();

    char *mediaFolder;   // env-var $MUSIC or $VIDEOS depending on extension
    std::string recode;  // recode to different format
    std::string quality; // quality of video or audio (bestvideo+bestuadio / bestaudio)
    if (std::regex_match(filename.string(), std::regex(".+\\.(mp3|flac|aac|m4a|opus|vorbis|wav)$")))
    {
        mediaFolder = std::getenv("MUSIC");
        recode = "-x --audio-format "s + extension;
        quality = "bestaudio";
    }
    else if (std::regex_match(filename.string(), std::regex(".+\\.(mp4|flv|ogg|webm|mkv|avi)$")))
    {
        mediaFolder = std::getenv("VIDEOS");
        recode = "--recode-video "s + extension;
        quality = "bestvideo+bestaudio";
    }
    else
    {
        std::cerr << "Invalid file format! " << extension << "\n";
        std::exit(1);
    }

    filepath = fs::absolute(fileop::resolve_filepath(filepath, mediaFolder));
    std::cout << filepath.string() << "." << extension << "\n";

    URL = (argc < 3 || (verbose && argc < 4)) ? get_URL() : argv[2];
    bool is_valid_URL = (!std::regex_match(URL, std::regex(R"(^(https?:\/\/)?([\da-z\.-]+)\.([a-z\.]{2,6})([\/\w \.-]*)*\/?$)")));
    if (is_valid_URL)
    {
        char input;
        do
        {
            std::cout << "'" << URL << "' is not a valid URL."
                      << " Search \"" << URL
                      << "\" on youtube instead?(y/n): ";
            std::cin >> input;

            if ((input == 'y') || (input == 'n'))
                break;
            std::cout << "entered: '" << input << "'\n";
            std::cout << "You may only type 'y' or 'n'.\n";
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        } while (true);

        if (input == 'y')
            URL = "'ytsearch:" + URL + "'";
        else
            std::exit(1);
    }

    std::string command = "youtube-dl -f "s + quality + " "s + URL + " "s + recode + " -o '" + filepath.string() + ".%(ext)s'" + " --no-mtime"s;
    if (verbose)
    {
        command += " -v";
        std::cout << command << "\n";
    }
    std::system(command.c_str());
}