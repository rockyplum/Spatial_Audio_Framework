# Spatial_Audio_Framework

A Spatial Audio Framework (SAF) written in C. The framework includes functions for performing Vector-Base Amplitude Panning (VBAP), Spherical Harmonic Transforms (SHT), and Higher-order Ambisonics (HOA); among others.

## Getting Started

To include this framework in a project, simply add the following code:

```
Spatial_Audio_Framework/framework
```

And add:

```
Spatial_Audio_Framework/framework/include
```

to the header search paths.

For the examples that support SOFA loading capabilities, statically built netcdf libraries must also be placed in:

```
Spatial_Audio_Framework/dependencies
```

Windows users must also install Intel's MKL, which can be freely acquired from
* [Intel MKL](https://software.intel.com/en-us/articles/free-ipsxe-tools-and-libraries)

## Examples

Several examples have also been included:

```
Spatial_Audio_Framework/examples/ambi_bin
Spatial_Audio_Framework/examples/ambi_dec
Spatial_Audio_Framework/examples/ambi_drc
Spatial_Audio_Framework/examples/ambi_enc
Spatial_Audio_Framework/examples/array2sh
Spatial_Audio_Framework/examples/binauraliser
Spatial_Audio_Framework/examples/panner
Spatial_Audio_Framework/examples/powermap
Spatial_Audio_Framework/examples/rotator
Spatial_Audio_Framework/examples/sldoa
Spatial_Audio_Framework/examples/upmix
```

* **ambi_bin** - a binaural Ambisonic decoder with built-in rotator
* **ambi_dec** - a frequency-dependent Ambisonic decoder (AllRAD, EPAD, MMD etc)
* **ambi_drc** - a frequency-dependent dynamic range compressor (DRC) for spherical harmonic signals (aka Ambisonic signals)
* **ambi_enc** - a simple Ambisonic encoder/panner
* **array2sh** - converts microphone array signals into spherical harmonic signals (aka Ambisonic signals)
* **binauraliser** - convolves input audio with interpolated HRTFs, which can be optionally loaded from a SOFA file
* **panner** - a frequency-dependent VBAP panner
* **powermap** - sound-field visualiser using beamformers (PWD, MVDR) or sub-space methods (MUSIC)
* **rotator** - rotates spherical harmonic signals (aka Ambisonic signals) given yaw-pitch-roll angles
* **sldoa** - spatially-localised direction of arrival estimator
* **upmix** - a (soon to be) collection of upmixing algorithms (currently only stereo to 5.x upmixing)

### GUI implementations

Many of these examples have been intergrated into VST audio plug-ins using the JUCE framework and can be found [here](http://research.spa.aalto.fi/projects/sparta_vsts/).

## Authors

* **Leo McCormack** - C programmer and DSP researcher (contact: leo.mccormack@aalto.fi)
* **Symeon Delikaris-Manias** - DSP researcher
* **Archontis Politis** - DSP researcher

## License

This framework is licensed under the [ISC license](https://choosealicense.com/licenses/isc/). However, it also includes the 'alias-free STFT' implementation by Juha Vilkamo (MIT license), which can be found [here](https://github.com/jvilkamo/afSTFT); and the 'convhull_3d' header only Convex Hull implementation by Leo McCormack (MIT license), which can be found [here](https://github.com/leomccormack/convhull_3d).

