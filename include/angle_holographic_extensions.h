///These are ultra non-standard non-GL extensions that have been added to ANGLE to better-support hololens interop added by Michael L. Farrell (mlfarrell)
///Many of these are simple pass-through's to the hololens SDK that cannot be done with existing GL APIs

#include <wrl.h>
#include <windows.perception.spatial.h>
#include <windows.foundation.h>

struct ID3D11Device;
struct ID3D11DeviceContext;

__declspec(dllimport) Microsoft::WRL::ComPtr<ABI::Windows::Perception::IPerceptionTimestamp> AngleHolographicGetCurrentPredictionTimestamp();
__declspec(dllimport) void AngleHolographicSetCurrentFocusPointParameters(const float pos[3], const float normal[3], const float velocity[3]);

__declspec(dllimport) Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IReference<ABI::Windows::Perception::Spatial::SpatialBoundingFrustum>> AngleHolographicGetCurrentBoundingFrustum();

__declspec(dllimport) ID3D11Device *AngleHolographicGetCurrentDevice();
__declspec(dllimport) ID3D11DeviceContext *AngleHolographicGetCurrentDeviceContext();

