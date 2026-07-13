namespace RTXVideoConverter.Core.Models;

public sealed record WindowPlacementState(int Width, int Height, int Dpi, bool IsMaximized)
{
    public bool IsValid => Width is >= 320 and <= 100_000 &&
                           Height is >= 240 and <= 100_000 &&
                           Dpi is >= 48 and <= 960;
}
