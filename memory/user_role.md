---
name: user-role
description: User's goals and learning context for this project
metadata:
  type: user
---

Lernprojekt: Der User möchte den Prozess bis zur Marktreife eines embedded Produkts aktiv nacherleben und verstehen — nicht nur das Endprodukt bauen. "Serienreife" ist ein ideelles Ziel, keine kommerzielle Absicht — echte Zertifizierungen (CE, FCC etc.) wären zu teuer und nicht nötig. Das Projekt ist potentiell Lehrmaterial für einen Fortgeschrittenenkurs — Code soll gut strukturiert und erklärbar sein, nicht nur funktionieren.

Das Gerät ist ausschließlich für den eigenen produktiven Einsatz gedacht (eigenes Heimnetz, eigene MainUnit). Sicherheitsfeatures werden nach Bedrohungsmodell bewertet: NVS-Verschlüsselung / JTAG-Deaktivierung / Secure Boot sind Produkthärtung für fremde Hände — hier nicht prioritär. Priorität hat: System funktioniert zuverlässig (MainUnit ↔ Node Integration), dann OTA (kein USB-Zugang im Betrieb nötig).

Neue Konzepte sollen kurz erklärt werden, wenn sie eingesetzt werden.
