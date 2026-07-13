using RTXVideoConverter.Core.Models;

namespace RTXVideoConverter.Core.Services;

public interface IWindowPlacementStore
{
    WindowPlacementState? Load();
    bool TrySave(WindowPlacementState state);
}
