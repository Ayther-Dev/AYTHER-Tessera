# Constitución — Núcleo de emulación multisistema

1. **Stack mínimo:** usar biblioteca estándar; cada dependencia externa exige justificación, versión y licencia documentadas.
2. **Spec primero:** especificar comportamientos y criterios de aceptación antes de implementarlos; cada cambio mantiene spec, código y tests alineados.
3. **Lógica independiente:** el núcleo funciona sin interfaz gráfica; observación y sustituciones acceden mediante contratos y no alteran el estado emulado.
4. **Tests obligatorios:** CI exige unidad, contratos y replay determinista con hashes de estado, vídeo y audio; cada bug corregido añade una regresión.
5. **Persistencia explícita:** formatos versionados, validados y sin punteros; guardado atómico, restauración entre procesos y rechazo explícito de incompatibilidades.
6. **Idioma consistente:** código, comentarios y logs propios nuevos en inglés; documentación y mensajes al usuario en español; eventos estructurados con códigos estables. Las fuentes externas conservan su idioma y avisos, con procedencia documentada.
