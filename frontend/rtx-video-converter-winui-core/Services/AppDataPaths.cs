namespace RTXVideoConverter.Core.Services;

public static class AppDataPaths
{
    public static string Root { get; } = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
        "RTX Video Converter");

    public static string History => Path.Combine(Root, "history.json");
    public static string Theme => Path.Combine(Root, "theme.txt");
}
