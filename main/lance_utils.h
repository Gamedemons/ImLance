#pragma once

namespace lance 
{
	// Application Utilities

	std::vector<std::string> getFileNames(char path[]);
	std::vector<std::string> getFileNames(std::string inputFiles);

	long long getFileSize(std::string path);

	std::string getFileContents(std::string path);

	long getCurrentTime(char type);

	long getWordCount(std::string str);
	long getLineCount(std::string str);

	std::string formatChapter(
		std::string str, 
		bool removeCheck,
		char removeText[],
		bool replaceCheck,
		char replaceText1[],
		char replaceText2[],
		int blankLineOption
	);

	// fRemoveText
	std::string fRemoveText(std::string str, bool option, char remove[]);

	// fReplaceText
	std::string fReplaceText(std::string str, bool option, char replaceText1[], char replaceText2[]);

	// fRenameFile
	std::string fRenameFile(
		bool renameCheck,
		std::string str,
		bool pathVisible,
		char renamerText1[],
		char renamerText2[],
		char renamerText3[],
		char renamerText4[],
		char renamerText5[]
	);

	std::string extractOldName(std::string str, bool pathVisible);

	// fRemoveLines
	std::string fRemoveLines(std::string str, int option);

	std::string removeExtraEmptyLines(std::string str);
	std::string removeEmptyLines(std::string str);
	std::string removeEmptySpaces(std::string str);

	void ltrim(std::string& s);
	void rtrim(std::string& s);
	void trim(std::string& s);

	// Type Conversions
	short int toShint(short int x);
	short int toShint(double x);
	float toFloat(double x);


	//Console
	static void ShowExampleAppConsole(bool* p_open);
}