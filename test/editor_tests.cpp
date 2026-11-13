#include <doctest/doctest.h>
#include <editor.hpp>
#include <sourcefile.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace stdfs = std::filesystem;

std::string readFile(const stdfs::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

// Writes the file as another program would, its modification time is moved on, so it differs also on file systems with
// a coarse time resolution
void writeFile(const stdfs::path& path, const std::string& content)
{
    std::error_code ec;
    const auto before = stdfs::last_write_time(path, ec);
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output << content;
    }
    if (!ec)
        stdfs::last_write_time(path, before + std::chrono::seconds(2));
}

std::vector<stdfs::path> filesIn(const stdfs::path& directory)
{
    std::vector<stdfs::path> files;
    std::error_code ec;
    for (const auto& entry : stdfs::directory_iterator(directory, ec))
        files.push_back(entry.path());
    std::ranges::sort(files);
    return files;
}

// A folder with a source file for one test case, ctest runs them in parallel
struct Source
{
    stdfs::path directory;
    stdfs::path file;

    Source(const std::string& name, const std::string& content)
        : directory(stdfs::temp_directory_path() / ("cadmium-editor-test-" + name))
        , file(directory / "source.8o")
    {
        stdfs::remove_all(directory);
        stdfs::create_directories(directory);
        std::ofstream output(file, std::ios::binary);
        output << content;
    }

    ~Source()
    {
        std::error_code ec;
        stdfs::permissions(file, stdfs::perms::owner_write, stdfs::perm_options::add, ec);
        stdfs::remove_all(directory, ec);
    }

    stdfs::path history() const { return directory / "history"; }
};

const std::string original = ": main\n    jump main\n";
const std::string edited = ": main\n    v0 := 1\n    jump main\n";

}  // namespace

TEST_CASE("Only a writable file with the text as content becomes the disk file")
{
    Source source("taken", original);
    SourceFile editor;
    editor.setText(original);
    editor.setDiskFile(source.file.string());
    CHECK(editor.diskFile() == source.file.string());

    // a source from somewhere else (like a cartridge) beside a file with another version
    editor.setText(edited);
    editor.setDiskFile(source.file.string());
    CHECK(editor.diskFile().empty());

    editor.setText(original);
    stdfs::permissions(source.file, stdfs::perms::owner_write | stdfs::perms::group_write | stdfs::perms::others_write, stdfs::perm_options::remove);
    editor.setDiskFile(source.file.string());
    CHECK(editor.diskFile().empty());
}

TEST_CASE("Changes are saved, Windows line endings are kept")
{
    Source source("crlf", ": main\r\n    jump main\r\n");
    SourceFile editor;
    editor.setText(original);
    editor.setDiskFile(source.file.string());
    REQUIRE(editor.diskFile() == source.file.string());
    CHECK_FALSE(editor.hasUnsavedChanges());
    editor.setText(edited);
    CHECK(editor.hasUnsavedChanges());
    CHECK(editor.autoSave());
    CHECK_FALSE(editor.hasUnsavedChanges());
    CHECK(readFile(source.file) == ": main\r\n    v0 := 1\r\n    jump main\r\n");
    CHECK_FALSE(stdfs::exists(source.file.string() + ".cadmium-saving"));
}

TEST_CASE("A file changed on disk is not saved over automatically, Sys+S does it")
{
    Source source("conflict", original);
    SourceFile editor;
    editor.setText(original);
    editor.setDiskFile(source.file.string());
    writeFile(source.file, "# changed elsewhere\n");
    editor.setText(edited);
    CHECK_FALSE(editor.autoSave());
    CHECK_FALSE(editor.saveError().empty());
    CHECK(readFile(source.file) == "# changed elsewhere\n");
    CHECK(editor.save());
    CHECK(editor.saveError().empty());
    CHECK(readFile(source.file) == edited);
}

TEST_CASE("The opened version is copied into the history before the first save")
{
    Source source("history", original);
    SourceFile editor;
    editor.setHistoryDirectory(source.history().string());
    editor.setText(original);
    editor.setDiskFile(source.file.string());
    editor.setText(edited);
    REQUIRE(editor.autoSave());
    auto copies = filesIn(source.history());
    REQUIRE(copies.size() == 1);
    CHECK(readFile(copies.front()) == original);
    CHECK(copies.front().extension() == ".8o");

    // later saves of the same opening add no copy
    editor.setText(original + "# more\n");
    REQUIRE(editor.autoSave());
    CHECK(filesIn(source.history()).size() == 1);

    // only the newest copies of a file are kept
    const auto name = copies.front().filename().string();
    const auto prefix = name.substr(0, name.size() - std::string("20260101-000000-000.8o").size());
    for (int i = 0; i < SourceFile::MAX_HISTORY_VERSIONS + 2; ++i) {
        std::ofstream old(source.history() / (prefix + "20200101-0000" + (i < 10 ? "0" : "") + std::to_string(i) + "-000.8o"));
        old << "old";
    }
    editor.setDiskFile(source.file.string());
    editor.setText(edited);
    REQUIRE(editor.autoSave());
    copies = filesIn(source.history());
    CHECK(copies.size() == SourceFile::MAX_HISTORY_VERSIONS);
    CHECK(readFile(copies.back()) == original + "# more\n");
}

TEST_CASE("Revert to the opened version")
{
    Source source("revert", original);
    SourceFile editor;
    editor.setText(original);
    editor.setDiskFile(source.file.string());
    CHECK_FALSE(editor.canRevertToOpened());
    editor.setText(edited);
    REQUIRE(editor.autoSave());
    CHECK(editor.canRevertToOpened());
    editor.revertToOpened();
    CHECK(editor.getText() == original);
    CHECK(editor.hasUnsavedChanges());
    REQUIRE(editor.autoSave());
    CHECK(readFile(source.file) == original);
}

TEST_CASE("A change on disk is reloaded, with own unsaved changes it is a conflict")
{
    Source source("reload", original);
    SourceFile editor;
    editor.setText(original);
    editor.setDiskFile(source.file.string());

    writeFile(source.file, edited);
    editor.checkDiskFile();
    CHECK(editor.getText() == edited);
    CHECK_FALSE(editor.hasUnsavedChanges());
    CHECK_FALSE(editor.isChangedOnDisk());

    // a file that was only touched changes nothing
    writeFile(source.file, edited);
    editor.checkDiskFile();
    CHECK_FALSE(editor.hasUnsavedChanges());

    editor.setText(edited + "# mine\n");
    writeFile(source.file, edited + "# theirs\n");
    editor.checkDiskFile();
    CHECK(editor.getText() == edited + "# mine\n");
    CHECK(editor.isChangedOnDisk());
    CHECK_FALSE(editor.saveError().empty());
    editor.reloadFromDisk();
    CHECK(editor.getText() == edited + "# theirs\n");
    CHECK_FALSE(editor.isChangedOnDisk());
    CHECK(editor.saveError().empty());
    CHECK_FALSE(editor.hasUnsavedChanges());
}

TEST_CASE("The editor opens further files, compiling saves them and compiles the main file")
{
    Source source("project", ": main\n    :include \"lib.8o\"\n    loop again\n");
    const auto main = source.file.string();
    const auto lib = (source.directory / "lib.8o").string();
    {
        std::ofstream output(lib, std::ios::binary);
        output << "    v0 := 1\n";
    }
    Editor editor;
    editor.setText(readFile(source.file));
    editor.setFilename(main);
    editor.setDiskFile(main);
    editor.compile();
    CHECK_MESSAGE(!editor.compiler().isError(), editor.compiler().compileResult().errorMessage);

    // another file opens as the current one, opening it again switches to it
    CHECK_FALSE(editor.openFile((source.directory / "missing.8o").string()));
    REQUIRE(editor.openFile(lib));
    CHECK(editor.files().size() == 2);
    CHECK(editor.currentFile().name() == "lib.8o");
    CHECK(editor.title().find("main: source.8o") != std::string::npos);
    editor.switchTo(editor.mainFile());
    REQUIRE(editor.openFile(lib, 1, 5));
    CHECK(editor.files().size() == 2);
    CHECK(editor.currentFile().name() == "lib.8o");
    CHECK(editor.line() == 1);
    CHECK(editor.column() == 5);

    // an edit of the included file changes the revision, compiling saves it first, so its error is found
    const auto revision = editor.contentRevision();
    editor.currentFile().setText("    v0 := 1\n    v9 += vz\n");
    CHECK(editor.contentRevision() > revision);
    CHECK_FALSE(editor.isCompiledUpToDate());
    editor.compile();
    CHECK(editor.isCompiledUpToDate());
    CHECK(readFile(lib) == "    v0 := 1\n    v9 += vz\n");
    REQUIRE(editor.compiler().isError());
    const auto& location = editor.compiler().compileResult().locations.front();
    CHECK(stdfs::path(location.file).filename() == "lib.8o");
    CHECK(location.line == 2);
    // the error location opens the file at its line (what a click on it does)
    editor.switchTo(editor.mainFile());
    REQUIRE(editor.openFile(location.file, location.line, location.column));
    CHECK(editor.currentFile().name() == "lib.8o");
    CHECK(editor.line() == 2);

    // the main file can't be closed, another file can, its changes are saved, the revision still grows
    editor.switchTo(editor.mainFile());
    editor.closeCurrentFile();
    CHECK(editor.files().size() == 2);
    editor.switchTo(*editor.files()[1]);
    editor.currentFile().setText("    v0 := 3\n");
    const auto beforeClose = editor.contentRevision();
    editor.closeCurrentFile();
    CHECK(editor.files().size() == 1);
    CHECK(&editor.currentFile() == &editor.mainFile());
    CHECK(readFile(lib) == "    v0 := 3\n");
    CHECK(editor.contentRevision() > beforeClose);

    // a new main text closes everything
    REQUIRE(editor.openFile(lib));
    editor.setText(": main\n");
    CHECK(editor.files().size() == 1);
    CHECK(editor.getText() == ": main\n");
}

TEST_CASE("Include lines link to their source files, the compilation knows the files of the build")
{
    Source source("includes", ": main\n    :include \"lib.8o\"  # the library\n    # :include \"other.8o\"\n    :include \"sprite.png\"\n    loop again\n");
    const auto main = source.file.string();
    const auto lib = (source.directory / "lib.8o").string();
    const auto other = (source.directory / "other.8o").string();
    {
        std::ofstream output(lib, std::ios::binary);
        output << "    v0 := 1\n";
    }
    {
        std::ofstream output(other, std::ios::binary);
        output << "    v1 := 1\n";
    }
    Editor editor;
    editor.setText(readFile(source.file));
    editor.setFilename(main);
    editor.setDiskFile(main);
    const auto& text = editor.getText();

    // the file name of the include is the link, the rest of the line isn't
    const auto name = static_cast<uint32_t>(text.find("lib.8o"));
    auto link = editor.includeLinkAt(editor.mainFile(), name + 2);
    REQUIRE(link.has_value());
    CHECK(link->start == name);
    CHECK(link->end == name + 6);
    CHECK(link->label == "Open lib.8o");
    CHECK_FALSE(editor.includeLinkAt(editor.mainFile(), static_cast<uint32_t>(text.find("the library"))).has_value());
    // an include in a comment and an image are no links
    CHECK_FALSE(editor.includeLinkAt(editor.mainFile(), static_cast<uint32_t>(text.find("other.8o"))).has_value());
    CHECK_FALSE(editor.includeLinkAt(editor.mainFile(), static_cast<uint32_t>(text.find("sprite.png"))).has_value());
    link->open();
    CHECK(editor.currentFile().name() == "lib.8o");

    // the compilation knows which files belong to the build (the image is missing, so drop its include)
    editor.switchTo(editor.mainFile());
    editor.setText(": main\n    :include \"lib.8o\"\n    loop again\n");
    editor.setDiskFile(main);
    editor.compile();
    CHECK_MESSAGE(!editor.compiler().isError(), editor.compiler().compileResult().errorMessage);
    REQUIRE(editor.openFile(lib));
    CHECK(editor.isInBuild(editor.currentFile()));
    REQUIRE(editor.openFile(other));
    CHECK_FALSE(editor.isInBuild(editor.currentFile()));
    CHECK(editor.isInBuild(editor.mainFile()));

    // a change of an included file that isn't open needs a compilation
    editor.switchTo(editor.mainFile());
    editor.setText(": main\n    :include \"lib.8o\"\n    loop again\n");
    editor.setDiskFile(main);
    editor.compile();
    REQUIRE(editor.isCompiledUpToDate());
    writeFile(lib, "    v9 += vz\n");
    editor.checkDiskFile(true);
    CHECK_FALSE(editor.isCompiledUpToDate());
    editor.compile();
    CHECK(editor.compiler().isError());
}

TEST_CASE("A program loaded from the project folder keeps the open files of the project")
{
    Source source("project-folder", ": main\n    :include \"lib.8o\"\n    loop again\n");
    const auto main = source.file.string();
    const auto lib = (source.directory / "lib.8o").string();
    const auto other = (source.directory / "sub" / "other.8o").string();
    writeFile(lib, "    v0 := 1\n");
    stdfs::create_directories(source.directory / "sub");
    writeFile(other, ": main\n    v1 := 2\n    loop again\n");
    Source outside("project-outside", ": main\n    jump main\n");

    Editor editor;
    CHECK(editor.treeRoot().empty());
    editor.setText(readFile(source.file), main);
    CHECK(editor.mainFile().path() == main);
    // without a project folder the tree shows the folder of the main file
    CHECK(editor.treeRoot() == source.directory.string());
    editor.setProjectFolder(source.directory.string() + "/");
    CHECK(editor.projectFolder() == source.directory.string());
    CHECK(editor.treeRoot() == source.directory.string());
    CHECK(editor.isInProject(other));
    CHECK_FALSE(editor.isInProject(source.directory.string()));
    CHECK_FALSE(editor.isInProject(outside.file.string()));
    CHECK_FALSE(editor.isInProject(""));

    // another main file of the project: the open files stay, an open one becomes the main file
    REQUIRE(editor.openFile(lib));
    REQUIRE(editor.openFile(other));
    REQUIRE(editor.openFile(outside.file.string()));
    editor.currentFile().setText(": main\n    jump main\n    # edited\n");
    editor.compile();
    const auto revision = editor.contentRevision();
    editor.setText(readFile(other), other);
    CHECK(editor.files().size() == 3);
    CHECK(&editor.currentFile() == &editor.mainFile());
    CHECK(editor.mainFile().path() == other);
    CHECK(editor.contentRevision() > revision);
    CHECK_FALSE(editor.isCompiledUpToDate());
    // the file outside of the project was saved and closed
    CHECK(readFile(outside.file) == ": main\n    jump main\n    # edited\n");
    CHECK(std::ranges::none_of(editor.files(), [&](const auto& file) { return file->path() == outside.file.string(); }));

    // a main file that isn't open is added, Windows line endings don't matter
    for (const auto& file : editor.files()) {
        if (file->path() == main) {
            editor.switchTo(*file);
            break;
        }
    }
    REQUIRE(editor.currentFile().path() == main);
    editor.closeCurrentFile();
    CHECK(editor.files().size() == 2);
    writeFile(main, ": main\r\n    :include \"lib.8o\"\r\n    loop again\r\n");
    editor.setText(readFile(source.file), main);
    editor.setFilename(main);
    CHECK(editor.files().size() == 3);
    CHECK(editor.mainFile().path() == main);
    CHECK(editor.mainFile().getText() == ": main\n    :include \"lib.8o\"\n    loop again\n");
    CHECK(editor.mainFile().diskFile() == main);
    editor.compile();
    CHECK_MESSAGE(!editor.compiler().isError(), editor.compiler().compileResult().errorMessage);

    // the tree marks the main file and the included ones
    CHECK(editor.fileMark(main, false).suffix == "(main)");
    CHECK(editor.fileMark(lib, false).suffix == "(in build)");
    CHECK(editor.fileMark(other, false).suffix.empty());
    CHECK(editor.fileMark(source.directory.string(), true).suffix.empty());

    // a program from outside of the project closes everything
    editor.setText(readFile(outside.file), outside.file.string());
    CHECK(editor.files().size() == 1);
    CHECK(editor.mainFile().path() == outside.file.string());
    // a text without file too
    editor.setProjectFolder({});
    CHECK_FALSE(editor.isInProject(other));
}

TEST_CASE("The main file of a folder is the program no other source includes")
{
    Source source("find-main", "    v0 := 1\n");
    const auto dir = source.directory;
    // one source is the program, whatever its name
    CHECK(Editor::findMainFile(dir.string()) == source.file.string());
    // several: the one no other includes, includes in comments don't count
    writeFile(dir / "game.8o", ": main\n    :include \"source.8o\"\n    :include \"sub/lib.8o\" # :include \"tools.8o\"\n");
    writeFile(dir / "tools.8o", "    v2 := 3\n");
    writeFile(dir / ".hidden.8o", ": main\n");
    stdfs::create_directories(dir / "sub");
    writeFile(dir / "sub" / "lib.8o", ": main\n");
    CHECK(Editor::findMainFile(dir.string()) == "");
    writeFile(dir / "game.8o", ": main\n    :include \"source.8o\"\n    :include \"tools.8o\" :include \"sub/lib.8o\"\n");
    CHECK(Editor::findMainFile(dir.string()) == (dir / "game.8o").string());
    // several that aren't included: the one named like the folder
    writeFile(dir / "test.8o", ": main\n    :include \"tools.8o\"\n");
    CHECK(Editor::findMainFile(dir.string()) == "");
    writeFile(dir / "cadmium-editor-test-find-main.8o", ": main\n    :include \"game.8o\"\n");
    writeFile(dir / "other.8o", ": main\n");
    CHECK(Editor::findMainFile(dir.string() + "/") == (dir / "cadmium-editor-test-find-main.8o").string());
    CHECK(Editor::findMainFile((dir / "missing").string()) == "");
}

TEST_CASE("A project file picked while the main file is from outside becomes the main file")
{
    Source source("adopt-main", ": main\n    :include \"lib.8o\"\n    loop again\n");
    const auto main = source.file.string();
    const auto lib = (source.directory / "lib.8o").string();
    writeFile(lib, "    v0 := 1\n");
    writeFile(source.directory / "notes.txt", "notes\n");

    Editor editor;
    std::vector<std::string> loaded;
    editor.setMainFileHandler([&](const std::string& path) {
        loaded.push_back(path);
        editor.setText(readFile(path), path);
    });
    // the startup text isn't edited, it is replaced
    editor.setText(": main\n    jump main");
    editor.setProjectFolder(source.directory.string());
    editor.pickFile(main);
    REQUIRE(loaded.size() == 1);
    CHECK(loaded.front() == main);
    CHECK(editor.files().size() == 1);
    CHECK(editor.mainFile().path() == main);
    // with a project main file picked files are only opened, also other files than sources
    editor.pickFile(lib);
    editor.pickFile((source.directory / "notes.txt").string());
    CHECK(loaded.size() == 1);
    CHECK(editor.files().size() == 3);
    CHECK(editor.mainFile().path() == main);

    // an edited text without file is kept
    editor.setText(": main\n    jump main");
    editor.currentFile().setText(": main\n    v1 := 1\n    jump main");
    editor.pickFile(main);
    CHECK(loaded.size() == 1);
    CHECK(editor.files().size() == 2);
    CHECK(editor.mainFile().path().empty());
    // without a project folder nothing changes the main file
    editor.setText(": main\n    jump main");
    editor.setProjectFolder({});
    editor.pickFile(main);
    CHECK(loaded.size() == 1);
    CHECK(editor.mainFile().path().empty());
}

TEST_CASE("A source defines main with a label line")
{
    CHECK(Editor::definesMain(": main\n"));
    CHECK(Editor::definesMain("# test\n\t:  main # start\n    jump main"));
    CHECK(Editor::definesMain("    : main"));
    CHECK(Editor::definesMain(": main\r\n"));
    CHECK_FALSE(Editor::definesMain(""));
    CHECK_FALSE(Editor::definesMain("# : main\n"));
    CHECK_FALSE(Editor::definesMain(": mainloop\n"));
    CHECK_FALSE(Editor::definesMain(":main\n"));
    CHECK_FALSE(Editor::definesMain("    jump main\n"));
}

TEST_CASE("A shown program of the project becomes the main file, included files from outside stay open")
{
    // like a test suite opened in its src/tests folder, the tests include ../utils
    Source source("follow-program", "");
    const auto tests = source.directory / "tests";
    const auto first = (tests / "1-first.8o").string();
    const auto second = (tests / "2-second.8o").string();
    const auto helper = (tests / "helper.8o").string();
    const auto utils = (source.directory / "utils" / "lib.8o").string();
    stdfs::create_directories(tests);
    stdfs::create_directories(source.directory / "utils");
    writeFile(first, ": main\n    :include \"../utils/lib.8o\"\n    loop again\n");
    writeFile(second, "# second\n: main\n    :include \"../utils/lib.8o\"\n    v1 := 2\n    loop again\n");
    writeFile(helper, ": helper\n    return\n");
    writeFile(utils, "    v0 := 1\n");

    Editor editor;
    std::vector<std::string> loaded;
    editor.setMainFileHandler([&](const std::string& path) {
        loaded.push_back(path);
        editor.setFilename(path);  // as the app does, includes are resolved from it
        editor.setText(readFile(path), path);
    });
    editor.setProjectFolder(tests.string());
    editor.setFilename(first);
    editor.setText(readFile(first), first);
    editor.compile();
    REQUIRE_MESSAGE(!editor.compiler().isError(), editor.compiler().compileResult().errorMessage);

    // an included file or one without main doesn't change the build
    REQUIRE(editor.openFile(utils));
    editor.followShownProgram();
    REQUIRE(editor.openFile(helper));
    editor.followShownProgram();
    CHECK(loaded.empty());
    CHECK(editor.mainFile().path() == first);

    // a program is built once it is shown, the file from outside stays open
    REQUIRE(editor.openFile(second));
    editor.currentFile().setText("# second, edited\n: main\n    :include \"../utils/lib.8o\"\n    v1 := 2\n    loop again\n");
    editor.followShownProgram();
    REQUIRE(loaded.size() == 1);
    CHECK(loaded.back() == second);
    CHECK(editor.mainFile().path() == second);
    CHECK(&editor.currentFile() == &editor.mainFile());
    // its edit was saved before the program was loaded from disk
    CHECK(editor.mainFile().getText().starts_with("# second, edited\n"));
    CHECK(editor.files().size() == 4);
    CHECK(std::ranges::any_of(editor.files(), [&](const auto& file) { return file->path() == utils; }));
    editor.followShownProgram();
    CHECK(loaded.size() == 1);

    // back to the first one
    for (const auto& file : editor.files()) {
        if (file->path() == first) {
            editor.switchTo(*file);
            break;
        }
    }
    editor.followShownProgram();
    REQUIRE(loaded.size() == 2);
    CHECK(editor.mainFile().path() == first);

    // a file that got a main label is a program when it is shown the next time
    for (const auto& file : editor.files()) {
        if (file->path() == helper) {
            editor.switchTo(*file);
            break;
        }
    }
    editor.followShownProgram();
    editor.currentFile().setText(": main\n    loop again\n");
    editor.followShownProgram();
    CHECK(loaded.size() == 2);
    editor.switchTo(editor.mainFile());
    editor.followShownProgram();
    for (const auto& file : editor.files()) {
        if (file->path() == helper) {
            editor.switchTo(*file);
            break;
        }
    }
    editor.followShownProgram();
    REQUIRE(loaded.size() == 3);
    CHECK(editor.mainFile().path() == helper);

    // an edited text without file isn't given up for a program
    editor.setText(": main\n    jump main");
    editor.currentFile().setText(": main\n    v2 := 1\n    jump main");
    REQUIRE(editor.openFile(second));
    editor.followShownProgram();
    CHECK(loaded.size() == 3);
    CHECK(editor.mainFile().path().empty());
}

TEST_CASE("Back and forward return to the places left")
{
    Source source("navigation", ": main\n    :include \"lib.8o\"\n    v0 := 1\n    v1 := 2\n    v2 := 3\n    loop again\n");
    const auto main = source.file.string();
    const auto lib = (source.directory / "lib.8o").string();
    const auto other = (source.directory / "other.8o").string();
    writeFile(lib, "# lib\n    v3 := 4\n    v4 := 5\n");
    writeFile(other, "# other\n");

    Editor editor;
    editor.setFilename(main);
    editor.setText(readFile(source.file), main);
    editor.trackPlace();
    CHECK_FALSE(editor.canGoBack());
    CHECK_FALSE(editor.canGoForward());

    // the place in the main file is remembered when the include is shown
    editor.currentFile().code().goTo(3, 5);
    editor.trackPlace();
    REQUIRE(editor.openFile(lib));
    editor.currentFile().code().goTo(2, 3);
    editor.trackPlace();
    CHECK(editor.canGoBack());
    // a jump to an error location in the shown file is remembered too
    REQUIRE(editor.openFile(lib, 3, 1));
    editor.trackPlace();
    REQUIRE(editor.openFile(other));
    editor.trackPlace();

    editor.goBack();
    CHECK(editor.currentFile().path() == lib);
    CHECK(editor.line() == 3);
    editor.goBack();
    CHECK(editor.currentFile().path() == lib);
    CHECK(editor.line() == 2);
    CHECK(editor.column() == 3);
    editor.goBack();
    CHECK(editor.currentFile().path() == main);
    CHECK(editor.line() == 3);
    CHECK(editor.column() == 5);
    CHECK_FALSE(editor.canGoBack());
    // going back isn't remembered as a jump
    editor.trackPlace();
    CHECK_FALSE(editor.canGoBack());

    editor.goForward();
    CHECK(editor.currentFile().path() == lib);
    CHECK(editor.line() == 2);
    editor.trackPlace();
    editor.goForward();
    editor.goForward();
    CHECK(editor.currentFile().path() == other);
    CHECK_FALSE(editor.canGoForward());

    // a closed file is opened again, a new jump ends the way forward
    editor.goBack();
    editor.trackPlace();
    CHECK(editor.currentFile().path() == lib);
    editor.goForward();
    editor.trackPlace();
    REQUIRE(editor.currentFile().path() == other);
    editor.closeCurrentFile();
    editor.trackPlace();
    CHECK(editor.currentFile().path() != other);
    editor.goBack();
    editor.trackPlace();
    CHECK(editor.currentFile().path() == other);
    CHECK(editor.canGoForward());
    REQUIRE(editor.openFile(main, 1, 1));
    editor.trackPlace();
    CHECK_FALSE(editor.canGoForward());

    // places of files that are gone are skipped, lines are limited to the text
    editor.goBack();
    editor.trackPlace();
    CHECK(editor.currentFile().path() == other);
    REQUIRE(editor.openFile(lib));
    editor.trackPlace();
    REQUIRE(editor.openFile(main));
    editor.trackPlace();
    for (const auto& file : editor.files()) {
        if (file->path() == other) {
            editor.switchTo(*file);
            break;
        }
    }
    editor.closeCurrentFile();
    stdfs::remove(other);
    editor.trackPlace();
    editor.goBack();  // the lib, the main file was shown in between
    editor.trackPlace();
    CHECK(editor.currentFile().path() == lib);
    editor.currentFile().code().goTo(3, 1);
    editor.trackPlace();
    editor.currentFile().setText("# lib");
    REQUIRE(editor.openFile(main));
    editor.trackPlace();
    editor.goBack();
    CHECK(editor.currentFile().path() == lib);
    CHECK(editor.line() == 1);
}

TEST_CASE("New files are created in their folder and opened")
{
    Source source("new-file", ": main\n    jump main\n");
    Editor editor;
    editor.setText(readFile(source.file), source.file.string());
    editor.setProjectFolder(source.directory.string());
    const auto folder = source.directory.string();

    CHECK(editor.createFile(folder, "  ") == "Enter a file name");
    CHECK(editor.createFile(folder, "../escape.8o") == "The file must be in the folder");
    CHECK(editor.createFile(folder, "source.8o") == "There is already a file source.8o");
    CHECK(editor.createFile(folder, " lib ").empty());
    const auto lib = (source.directory / "lib.8o").string();
    CHECK(stdfs::is_regular_file(lib));
    CHECK(editor.currentFile().path() == lib);
    CHECK(editor.currentFile().diskFile() == lib);
    // other extensions are kept, missing folders are created
    CHECK(editor.createFile(folder, "data/notes.txt").empty());
    CHECK(stdfs::is_regular_file(source.directory / "data" / "notes.txt"));
    CHECK(editor.files().size() == 3);
    // the new file is saved like any other
    editor.currentFile().setText("some notes\n");
    CHECK(editor.autoSave());
    CHECK(readFile(source.directory / "data" / "notes.txt") == "some notes\n");
}

TEST_CASE("Fuzzy scores prefer file names, word starts and runs")
{
    CHECK(Editor::fuzzyScore("", "any/path.8o") == 0);
    CHECK(Editor::fuzzyScore("xyz", "lib/util.8o") == -1);
    CHECK(Editor::fuzzyScore("ilu", "lib/util.8o") == -1);  // the letters must be in order
    CHECK(Editor::fuzzyScore("UTIL", "lib/util.8o") > 0);    // no case
    // in the file name beats in the folder
    CHECK(Editor::fuzzyScore("lib", "lib/util.8o") < Editor::fuzzyScore("lib", "src/lib.8o"));
    // a run beats scattered letters, word starts beat letters inside of words
    CHECK(Editor::fuzzyScore("spr", "sprites.8o") > Editor::fuzzyScore("spr", "sound-parser.8o"));
    CHECK(Editor::fuzzyScore("gm", "game-main.8o") > Editor::fuzzyScore("gm", "gamma.8o"));
    CHECK(Editor::fuzzyScore("ps", "PlayerSprite.8o") > Editor::fuzzyScore("ps", "pause.8o"));
    // the whole name is best
    CHECK(Editor::fuzzyScore("util.8o", "lib/util.8o") > Editor::fuzzyScore("util.8o", "lib/utility.8o"));
    // letters over the folder and the name match too, the best places count
    CHECK(Editor::fuzzyScore("lu", "lib/util.8o") > 0);
    CHECK(Editor::fuzzyScore("gmain", "game-main.8o") > Editor::fuzzyScore("gmain", "gamemain.8o"));
    CHECK(Editor::fuzzyScore("ab", "xaxbab") == 1 + 1 + 5 + 20);
}

TEST_CASE("Quick open finds the text files of the folder, best first")
{
    Source source("quick-open", ": main\n    jump main\n");
    const auto dir = source.directory;
    stdfs::create_directories(dir / "lib");
    stdfs::create_directories(dir / ".git");
    writeFile(dir / "lib" / "util.8o", "    v0 := 1\n");
    writeFile(dir / "lib" / "sprites.8o", "    0x00\n");
    writeFile(dir / "sound-parser.8o", "    v1 := 1\n");
    writeFile(dir / "notes.txt", "notes\n");
    writeFile(dir / "logo.png", "png");
    writeFile(dir / ".git" / "config.8o", "hidden\n");
    Editor editor;
    editor.setText(readFile(source.file), source.file.string());
    editor.setProjectFolder(dir.string());
    REQUIRE(editor.openFile((dir / "notes.txt").string()));
    editor.openQuickOpen();
    CHECK(editor.isQuickOpenOpen());

    auto names = [&](const std::string& pattern) {
        std::string result;
        for (const auto& path : editor.quickOpenMatches(pattern))
            result += (result.empty() ? "" : " ") + stdfs::path(path).lexically_relative(dir).generic_string();
        return result;
    };
    // without a pattern the open files come first, the current one first, no images, nothing hidden
    CHECK(names("") == "notes.txt source.8o lib/sprites.8o lib/util.8o sound-parser.8o");
    CHECK(names("spr") == "lib/sprites.8o sound-parser.8o");
    CHECK(names("util") == "lib/util.8o");
    CHECK(names("lib") == "lib/util.8o lib/sprites.8o");
    CHECK(names("png").empty());
    CHECK(names("zzz").empty());
}
