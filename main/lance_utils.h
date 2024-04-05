#pragma once

namespace lance 
{
	// Application Utilities

	std::vector<std::string> getFileNames(char path[]);
	std::vector<std::string> getFileNames(std::string inputFiles);

	long long getFileSize(std::string path);

	std::string getFileContents(std::string path);

	long getCurrentTime(char type);

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

	// fRemoveLines
	std::string fRemoveLines(std::string str, int option);

	std::string removeExtraEmptyLines(std::string str);
	std::string removeEmptyLines(std::string str);
	std::string removeEmptySpaces(std::string str);

	// Type Conversions

	short int toShint(short int x);
	short int toShint(double x);
	float toFloat(double x);

}