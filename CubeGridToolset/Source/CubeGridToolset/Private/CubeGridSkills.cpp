// YuriNK, 2026, All Rights Reserved
#include "CubeGridToolset.h"

#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FString GetSkillsRoot()
	{
		const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("CubeGridToolset"));
		return Plugin ? FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetContentDir(), TEXT("Skills"))) : FString();
	}

	bool IsSkillName(const FString& Name)
	{
		if (Name.IsEmpty()) return false;
		for (TCHAR Character : Name)
			if (!(FChar::IsAlnum(Character) || Character == TEXT('-'))) return false;
		return true;
	}
}

TArray<FCubeGridSkillInfo> UCubeGridToolset::GetSkillCatalog()
{
	TArray<FCubeGridSkillInfo> Skills;
	const FString Root = GetSkillsRoot();
	if (Root.IsEmpty())
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("CubeGridToolset plugin content directory is unavailable."));
		return Skills;
	}
	TArray<FString> Folders;
	IFileManager::Get().FindFiles(Folders, *FPaths::Combine(Root, TEXT("*")), false, true);
	Folders.Sort();
	for (const FString& Folder : Folders)
	{
		FString Content;
		if (!IsSkillName(Folder) || !FFileHelper::LoadFileToString(Content, *FPaths::Combine(Root, Folder, TEXT("SKILL.md")))) continue;
		FCubeGridSkillInfo Info;
		Info.Name = Folder;
		Info.ContentPath = FString::Printf(TEXT("Skills/%s/SKILL.md"), *Folder);
		TArray<FString> Lines;
		Content.ParseIntoArrayLines(Lines);
		for (const FString& Line : Lines)
		{
			if (Line.StartsWith(TEXT("description:")))
			{
				Info.Description = Line.Mid(12).TrimStartAndEnd();
				break;
			}
		}
		Skills.Add(MoveTemp(Info));
	}
	return Skills;
}

FString UCubeGridToolset::ReadSkill(const FString& SkillName, const FString& RelativePath)
{
	const FString Root = GetSkillsRoot();
	FString Relative = RelativePath;
	Relative.ReplaceInline(TEXT("\\"), TEXT("/"));
	if (Root.IsEmpty() || !IsSkillName(SkillName) || !FPaths::IsRelative(Relative) || Relative.Contains(TEXT(":")))
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Choose a bundled skill and a relative path within its directory."));
		return {};
	}
	const FString SkillRoot = FPaths::ConvertRelativePathToFull(FPaths::Combine(Root, SkillName));
	FString Target = FPaths::ConvertRelativePathToFull(FPaths::Combine(SkillRoot, Relative));
	FPaths::NormalizeFilename(Target);
	FString Prefix = SkillRoot;
	FPaths::NormalizeFilename(Prefix);
	Prefix += TEXT("/");
	if (!FPaths::CollapseRelativeDirectories(Target) || !Target.StartsWith(Prefix, ESearchCase::IgnoreCase) ||
		!IFileManager::Get().FileExists(*FPaths::Combine(SkillRoot, TEXT("SKILL.md"))))
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("The requested file must be inside a bundled skill directory."));
		return {};
	}
	const FString Extension = FPaths::GetExtension(Target).ToLower();
	if (Extension != TEXT("md") && Extension != TEXT("json") && Extension != TEXT("py") && Extension != TEXT("yaml"))
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("ReadSkill supports Markdown, JSON, Python, and YAML text resources."));
		return {};
	}
	const int64 Size = IFileManager::Get().FileSize(*Target);
	FString Content;
	if (Size < 0 || Size > 256 * 1024 || !FFileHelper::LoadFileToString(Content, *Target))
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Skill file is missing, unreadable, or larger than 256 KiB."));
		return {};
	}
	return Content;
}
