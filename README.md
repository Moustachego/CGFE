# CGFE-Reproduction

This repository is a **reproduction and experimental re-implementation** of range encoding and packet classification techniques proposed in prior research on **TCAM-based packet classification**, with a primary focus on **CGFE (Compact Gray-code-based Flow Encoding)**.

The goal of this project is to **faithfully reproduce the algorithms, encoding schemes, and evaluation logic** described in the referenced papers, validate their correctness, and explore practical implementation considerations in modern software environments.

---

## 📌 Project Scope

This project focuses on:

- Reproducing **range-to-Gray-code encoding** methods for TCAM/SRAM-based packet classification  
- Implementing **interval decomposition and encoding** for IP addresses and port ranges  
- Validating correctness through synthetic and real ACL rule sets  
- Exploring **space efficiency**, **rule expansion**, and **intersection behavior**  
- Providing a modular and extensible experimental framework for further research

This is **not an official implementation** and is **not affiliated with the original authors**.

---

## 📄 Related Papers (References)

This work is based on and inspired by the following publications:

1. **J. Graf, V. Demianiuk, P. Chuprikov, Y. Feiran, S. I. Nikolenko, and P. Eugster**  
   *CGFE: Efficient Range Encoding for TCAMs*  
   IEEE INFOCOM 2025 - IEEE Conference on Computer Communications,  
   London, United Kingdom, 2025, pp. 1–10.  
   DOI: 10.1109/INFOCOM55648.2025.11044571

2. **A. Bremler-Barr and D. Hendler**  
   *Space-Efficient TCAM-Based Classification Using Gray Coding*  
   IEEE Transactions on Computers, vol. 61, no. 1, pp. 18–30, Jan. 2012.  
   DOI: 10.1109/TC.2010.267

3. **K. Lakshminarayanan, A. Rangarajan, and S. Venkatachary**  
   *Algorithms for Advanced Packet Classification with Ternary CAMs*  
   ACM SIGCOMM Computer Communication Review,  
   vol. 35, no. 4, pp. 193–204, 2005.

---

## ⚠️ Disclaimer

- This repository is intended **for academic research and educational purposes only**
- The implementation is a **best-effort reproduction** based on the descriptions in the papers
- Any deviations, bugs, or inconsistencies are the responsibility of the author of this repository

---

## 🧪 Reproducibility Notes

- Some algorithmic details are **implicitly described or omitted** in the original papers  
- Where necessary, reasonable assumptions or design choices are made and documented in code comments  
- Experimental parameters may differ slightly from those reported in the original evaluations

---

## 📂 Repository Structure (Example)

src/
├── encoding/ # Range & Gray-code encoding logic
├── acl_rules/ # Input ACL rule sets
├── verification/ # Correctness checking & validation
├── experiments/ # Evaluation scripts
└── utils/ # Shared helper functions


---

## 📬 Contact

If you are one of the original authors and have concerns or suggestions regarding this reproduction, please feel free to reach out.

---

## 📜 License

This project is released under an open-source license (see `LICENSE` file for details).
