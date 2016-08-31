ANGLE -- mlfarrell's fork of ms-holographic-experimental
=====

This ia a public fork of ms-holographic-experimental.  It serves to get up and running as fast as possible on OpenGL+Hololens

Some things are scary in here..  But they work.  
Once ms-holographic-experimental gets its legs, this branch will become less relevant.

Goodbye..


## Remarks

Issues solved (that may or may not work on the experimental/official ms branch)

- Handle multiple holographic shaders
- Obtain individual stereo ModelView, Projection matrix pairs via glGetFloat API /w GL_HOLOGRAPHIC_MVP_MATRICES_ANGLE etc
- Offscreen non-stereo FBOs
- Added somewhat hacked angle_holographic_extensions.h include which contains access to certain Hololens SDK features
  - AngleHolographicGetCurrentPredictionTimestamp to obtain prediction timestamp for upcoming holographic frame
  - AngleHolographicSetCurrentFocusPointParameters to set image stabilization parameters for the holographic camera