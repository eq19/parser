---
sort: 22
spin: 31
span: 
suit: 113
description: 
---
# Electrodynamics (maps)

The term of electrodynamics is referring to Quantum Electrodynamics ([QED](https://en.wikipedia.org/wiki/Quantum_electrodynamics)) which is the [relativistic](https://en.wikipedia.org/wiki/Theory_of_relativity) [quantum field theory](https://en.wikipedia.org/wiki/Quantum_field_theory) of [Electromagnetism](https://en.wikipedia.org/wiki/Electrodynamics).

{% include list.liquid all=true %}

In essence, it describes how [light](https://en.wikipedia.org/wiki/Light) and [matter](https://en.wikipedia.org/wiki/Matter) interact and is **the first theory** where full agreement between [quantum mechanics](https://en.wikipedia.org/wiki/Quantum_mechanics) and [special relativity](https://en.wikipedia.org/wiki/Special_relativity) is achieved.

## Basic Transformation

The first formulation of a [quantum theory](https://en.wikipedia.org/wiki/Quantum_mechanics) describing radiation and matter interaction is attributed to British scientist [Paul Dirac](https://en.wikipedia.org/wiki/Paul_Dirac).

```note
Dirac described the quantization of the [electromagnetic field](https://en.wikipedia.org/wiki/Electromagnetic_field) as an ensemble of [harmonic oscillators](https://en.wikipedia.org/wiki/Harmonic_oscillator) with the introduction of the concept of [creation and annihilation operators](https://en.wikipedia.org/wiki/Creation_and_annihilation_operators) of particles. 

Based on Bethe's intuition and fundamental papers on the subject by [Shin'ichirō Tomonaga](https://en.wikipedia.org/wiki/Shin%27ichir%C5%8D_Tomonaga),[[16]](https://en.wikipedia.org/wiki/Quantum_electrodynamics#cite_note-tomonaga-16) [Julian Schwinger](https://en.wikipedia.org/wiki/Julian_Schwinger),[[17]](https://en.wikipedia.org/wiki/Quantum_electrodynamics#cite_note-schwinger1-17)[[18]](https://en.wikipedia.org/wiki/Quantum_electrodynamics#cite_note-schwinger2-18) [Richard Feynman](https://en.wikipedia.org/wiki/Richard_Feynman)[[1]](https://en.wikipedia.org/wiki/Quantum_electrodynamics#cite_note-feynman1-1)[[19]](https://en.wikipedia.org/wiki/Quantum_electrodynamics#cite_note-feynman2-19)[[20]](https://en.wikipedia.org/wiki/Quantum_electrodynamics#cite_note-feynman3-20) and [Freeman Dyson](https://en.wikipedia.org/wiki/Freeman_Dyson),[[21]](https://en.wikipedia.org/wiki/Quantum_electrodynamics#cite_note-dyson1-21)[[22]](https://en.wikipedia.org/wiki/Quantum_electrodynamics#cite_note-dyson2-22) it was finally possible to get fully [covariant](https://en.wikipedia.org/wiki/Lorentz_covariance) formulations that were finite at any order in a perturbation series of quantum electrodynamics.

The key components of Feynman's presentation of QED are [three basic actions](https://en.wikipedia.org/wiki/Quantum_electrodynamics#cite_note-feynbook-2)

- A [photon](https://en.wikipedia.org/wiki/Photon) goes from one place and time to another place and time.
- An [electron](https://en.wikipedia.org/wiki/Electron) goes from one place and time to another place and time.
- An electron emits or absorbs a photon at a certain place and time.

These actions are represented in the form of visual shorthand by the three basic elements of [diagrams](https://en.wikipedia.org/wiki/Feynman_diagram): a wavy line for the photon, a straight line for the electron and a junction of two straight lines and a wavy one for a vertex representing emission or absorption of a photon by an electron. _([Wikipedia](https://en.wikipedia.org/wiki/Quantum_electrodynamics))_
```

[![1_RMV1kvtEZ-o-_8WKQLnCSA](https://github.com/eq19/maps/assets/8466209/715b20a8-d5c2-4b72-8993-b696a8b25d23)](https://platosrealm.wordpress.com/2019/02/26/unsolved-problems-in-physics-baryon-asymmetry/)

QED is based on the above three building blocks and then using the probability amplitudes to calculate the probability of any such complex interaction.

```note
It turns out that the basic idea of QED can be communicated while assuming that the square of the total of the probability amplitudes mentioned above (P(A to B), E(C to D) and j) acts just like our everyday probability (a simplification made in [Feynman's book](https://github.com/eq19/maps/files/13930690/PhysRev.76.769.pdf)). Later on, this will be corrected to include specifically quantum-style mathematics, following Feynman.

The basic rules of probability amplitudes that will be used are:
- If an event can occur via a number of indistinguishable alternative processes (a.k.a. "virtual" processes), then its probability amplitude is the ***sum of the probability amplitudes of the alternatives***.
- If a virtual process involves a number of independent or concomitant sub-processes, then the probability amplitude of the total (compound) process is the ***product of the probability amplitudes of the sub-processes***.

The indistinguishability criterion in (a) is very important: it means that there is no observable feature present in the given system that in any way "reveals" which alternative is taken. In such a case, one cannot observe which alternative actually takes place without changing the experimental setup in some way (e.g. by introducing a new apparatus into the system). _([Wikipedia](https://en.wikipedia.org/wiki/Quantum_electrodynamics))_
```

[![First_Feynman_Diagram](https://github.com/eq19/maps/assets/8466209/c381a212-cf38-45db-a166-f8e8716886da)](https://github.com/eq19/maps/files/13930690/PhysRev.76.769.pdf)

They are related to our everyday ideas of probability by the simple rule that the probability of an event is the square of the length of the corresponding amplitude arrow.

```note
Feynman replaces complex numbers with spinning arrows, which start at emission and end at detection of a particle.

[![Feynmans_QED_probability_amplitudes](https://github.com/user-attachments/assets/df7b0132-ccc2-4372-8f4e-ca2211b7956c)](https://en.wikipedia.org/wiki/Quantum_electrodynamics#probability-amplitudes)

- The sum of all resulting arrows gives a final arrow whose length squared equals the probability of the event.
- In this diagram, light emitted by the source S can reach the detector at P by bouncing off the mirror (in blue) at various points.
- Each one of the paths has an arrow associated with it (whose direction changes uniformly with the time taken for the light to traverse the path).
- To correctly calculate the total probability for light to reach P starting at S, one needs to sum the arrows for all such paths.

The graph below depicts the total time spent to traverse each of the paths above.
```

![008869256_1-75ca18aad2faf65f52f4c7706d7d8bd3-768x994](https://github.com/eq19/maps/assets/8466209/7c7f069f-dcfb-439e-961e-68c63c76dc9d)

Feynman avoids exposing the reader to the mathematics of complex numbers by using a simple but accurate representation of them as arrows on a piece of paper or screen.

```note
Mathematically, QED is an [abelian](https://en.wikipedia.org/wiki/Abelian_group) [gauge theory](https://en.wikipedia.org/wiki/Gauge_theory) with the symmetry group [U(1)](https://en.wikipedia.org/wiki/U(1)), defined on [Minkowski space](https://en.wikipedia.org/wiki/Minkowski_space) (flat spacetime). The [gauge field](https://en.wikipedia.org/wiki/Gauge_field), which mediates the interaction between the charged [spin-1/2](https://en.wikipedia.org/wiki/Spin_(physics)) [fields](https://en.wikipedia.org/wiki/Field_(physics)), is the [electromagnetic field](https://en.wikipedia.org/wiki/Electromagnetic_field). The QED [Lagrangian](https://en.wikipedia.org/wiki/Lagrangian_(field_theory)) for a spin-1/2 field interacting with the electromagnetic field in natural units gives rise to the QED Action
```

[![image-18](https://github.com/eq19/maps/assets/8466209/f233cea8-77c7-4456-bc5c-65b0e98b3f73)](https://slidetodoc.com/particle-physics-michaelmas-term-2011-prof-mark-thomson-6/)

Finally, one has to compute P(A to B) and E(C to D) corresponding to the probability amplitudes for the photon and the electron respectively.

## Mapping Scheme

QED has served as the model and template for all subsequent quantum field theories. One such subsequent theory is Quantum Chromodynamics ([QCD](https://www.eq19.com/lexer/exponentiation/span15/)).

```note
Quantum Chromo Dynamics (in short QCD) is the field that studies the strong force between quarks . Like QED (Quantum Electro Dynamics) studies the electromagnetic force on the basis of quantum field theory, QCD does so as well. So we will find many similarities in applying fields, waves, interactions and how the force comes about. However, many of the processes in QCD cannot be calculated exactly. So it is not as advanced as QED.
```

![images (10)](https://github.com/eq19/maps/assets/8466209/05f120ec-515e-4457-9625-bf4a6d7ee28f)

![QCD](https://github.com/eq19/maps/assets/8466209/fb3bf635-808c-4105-b8d6-2821486ed219)

[![proton-1](https://github.com/eq19/maps/assets/8466209/b110ca02-ac35-400a-8c83-dfb23e1c1c4b)](https://bigthink.com/starts-with-a-bang/why-8-types-gluon/)

Within [a cycle](https://gist.github.com/eq19/0ce5848f7ad62dc46dedfaa430069857#true-prime-pairs) this scheme would generate the [prime platform](https://gist.github.com/eq19/0ce5848f7ad62dc46dedfaa430069857#primes-platform) which is performing ***the rank of 10 shapes*** starting with the primes 2,3,5,7. 

![qcd-620px](https://github.com/eq19/maps/assets/8466209/3d4c5903-0d79-4c11-8c89-1a62fedb8e29)

![qcd_together](https://github.com/eq19/maps/assets/8466209/86fe9814-2aa3-42ba-9549-6569858c4690)

Renormalizability has become an essential criterion for a quantum field theory to be considered as a viable one.

![axioms-12-01058-g004](https://github.com/eq19/maps/assets/8466209/b43f5e6e-8a2d-4cf2-bff8-f111c263e273)

All the theories describing fundamental interactions, except gravitation, whose quantum counterpart is only conjectural and presently under very active research, are renormalizable theories.

![I15-73-Feynman](https://github.com/eq19/maps/assets/8466209/1acfbce9-61d4-443e-9aba-45d09d86659f)

Via the [11 partitions](https://gist.github.com/eq19/0ce5848f7ad62dc46dedfaa430069857#the-%CE%B419-vs-18-scenario) as the lexer and [13 frames](https://gist.github.com/eq19/0ce5848f7ad62dc46dedfaa430069857#the-power-of-168-vs-618) as the parser we do a [recombination](https://gist.github.com/eq19/0ce5848f7ad62dc46dedfaa430069857#scheme-139) to build the [grammar](https://gist.github.com/eq19/0ce5848f7ad62dc46dedfaa430069857#the-prime-recycling-%CE%B6s) in 6 periods.

[![6 minor hexagons](https://github.com/eq19/maps/assets/8466209/1e7dc443-b7d8-44d9-8da0-5fe50dd7ee70)](https://www.hexspin.com/minor-hexagons/)

When recombination is occur then the prime **13 is forced to → 12** where the impact (Δ1) goes to 18+The tensor product of a triplet with an octet reducing to a deciquintuplet, an anti-sextet, and a triplet appears diagrammatically as ***a total of 24 states***.

[![Young_tableaux_17](https://github.com/eq19/maps/assets/8466209/6647c9e1-026f-4aaa-b3a0-102a274bbc46)
![Young_tableaux_18](https://github.com/eq19/maps/assets/8466209/9adba5c0-a334-4d06-8bc1-b52662bd7d12)](https://en.m.wikipedia.org/wiki/Clebsch%E2%80%93Gordan_coefficients_for_SU(3))

## Subsequent Theories

Using the same procedure, any direct product representation is easily reduced.

![InteractionVertexOfQED](https://github.com/eq19/maps/assets/8466209/821ddfaf-5e0e-4434-8ab0-2778d4a971d8)

![images (5)](https://github.com/eq19/maps/assets/8466209/d31951c3-ece1-4aff-88b3-b85f64e3e5e2)

From them, computations of probability amplitudes are straightforwardly given. An example is Compton scattering, with an electron and a photon undergoing elastic scattering.

![300px-Compton_qed](https://github.com/eq19/maps/assets/8466209/36f87779-9386-46cb-b934-d3a01e375346)

The coupling constant runs to infinity at finite energy, signalling a Landau pole.

![mqdefault](https://github.com/eq19/maps/assets/8466209/68994f5a-614a-4d63-9446-6acac3df2829)

From a modern perspective, we say that QED is not well defined as a quantum field theory to arbitrarily high energy.

![478517_2_En_18_Fig10_HTML](https://github.com/eq19/maps/assets/8466209/9a1b324a-db44-4044-8058-61bb61372ee4)

The entanglement implies that there remains a connection between the photon and the emitting atom after emission even in very strong fields.

![Diagrams-in-strong-field-quantum-electrodynamics-SFQED-versus-ordinary-quantum](https://github.com/eq19/maps/assets/8466209/d7775d3a-d9e2-4993-82be-4279f209f58d)

An argument by Freeman Dyson shows that the radius of convergence of the perturbation series in QED is zero.

```note
Despite the conceptual clarity of this Feynman approach to QED, almost no early textbooks follow him in their presentation. When performing calculations, it is much easier to work with the [Fourier transforms](https://en.wikipedia.org/wiki/Fourier_transform) of the [propagators](https://en.wikipedia.org/wiki/Propagator).
- Experimental tests of quantum electrodynamics are typically scattering experiments. In scattering theory, particles' [momenta](https://en.wikipedia.org/wiki/Momentum) rather than their positions are considered, and it is convenient to think of particles as being created or annihilated when they interact. Feynman diagrams then look the same, but the lines have different interpretations.
- The electron line represents an electron with a given energy and momentum, with a similar interpretation of the photon line. A vertex diagram represents the annihilation of one electron and the creation of another together with the absorption or creation of a photon, each having specified energies and momenta.

Using [Wick's theorem](https://en.wikipedia.org/wiki/Wick%27s_theorem) on the terms of the **Dyson series**, all the terms of the [S-matrix](https://en.wikipedia.org/wiki/S-matrix) for quantum electrodynamics can be computed through the technique of [Feynman diagrams](https://en.wikipedia.org/wiki/Feynman_diagrams). In this case, rules for drawing are the following
```

![a-Summary-of-the-Feynman-rules-Solid-line-represents-the-fermionic-propagator-G-0-pp](https://github.com/eq19/maps/assets/8466209/f3430964-392b-43f9-80fa-84eed55f474d)

Quantum electrodynamics also leads to predictions beyond perturbation theory. In the presence of very strong electric fields, it predicts that electrons and positrons will be spontaneously produced, so causing the decay of the field.

![axioms-12-01058-g002-550](https://github.com/eq19/maps/assets/8466209/66dc1ba2-cbfd-4063-9f65-b6f32ce21dfd)

![hq720 (1)](https://github.com/eq19/maps/assets/8466209/9a6fd867-6867-416a-9b36-126198f4d259)

[![1-quantum-electrodynamics-laguna-designscience-photo-library](https://github.com/eq19/maps/assets/8466209/968afaea-ff71-4a56-b9ff-423f4a9748d2)](https://en.wikipedia.org/wiki/Quantum_electrodynamics)

The problem is essentially that QED appears to suffer from quantum triviality issues. This is one of the motivations for embedding QED within a [Grand Unified Theory](https://www.eq19.com/syntax/exponentiation/span13/).
