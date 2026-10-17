/* SPDX-License-Identifier: GPL-3.0-only */
#include "status_copy.h"
#include <string.h>

typedef struct { const char *key, *en, *pt, *fr, *de, *it; } Entry;
/* Fixed first-party status keys from device_auth.c and xbox_live.c. Keep
 * protocol fields, server messages, account names and game titles out. */
static const Entry entries[] = {
    {"La autorizacion Xbox agoto el tiempo. Reintenta.", "Xbox authorization timed out. Try again.", "A autorização Xbox expirou. Tente de novo.", "L'autorisation Xbox a expiré. Réessayez.", "Xbox-Autorisierung abgelaufen. Erneut versuchen.", "Autorizzazione Xbox scaduta. Riprova."},
    {"La consulta a Xbox agoto el tiempo. Reintenta.", "Xbox request timed out. Try again.", "A consulta ao Xbox expirou. Tente de novo.", "La requête Xbox a expiré. Réessayez.", "Xbox-Anfrage abgelaufen. Erneut versuchen.", "Richiesta Xbox scaduta. Riprova."},
    {"Microsoft no acepta esta aplicacion", "Microsoft does not accept this app", "A Microsoft não aceita este aplicativo", "Microsoft refuse cette application", "Microsoft akzeptiert diese App nicht", "Microsoft non accetta questa app"},
    {"Microsoft no pudo autorizar la conexion", "Microsoft could not authorize the connection", "A Microsoft não pôde autorizar a conexão", "Microsoft n'a pas pu autoriser la connexion", "Microsoft konnte die Verbindung nicht autorisieren", "Microsoft non ha potuto autorizzare la connessione"},
    {"Microsoft no reconoce el codigo", "Microsoft does not recognize the code", "A Microsoft não reconhece o código", "Microsoft ne reconnaît pas le code", "Microsoft erkennt den Code nicht", "Microsoft non riconosce il codice"},
    {"Microsoft no pudo renovar el acceso", "Microsoft could not renew access", "A Microsoft não pôde renovar o acesso", "Microsoft n'a pas pu renouveler l'accès", "Microsoft konnte den Zugriff nicht erneuern", "Microsoft non ha potuto rinnovare l'accesso"},
    {"Microsoft no entrego renovacion de acceso", "Microsoft did not provide renewed access", "A Microsoft não renovou o acesso", "Microsoft n'a pas renouvelé l'accès", "Microsoft hat den Zugriff nicht erneuert", "Microsoft non ha rinnovato l'accesso"},
    {"Microsoft rechazo la autorizacion de conexion", "Microsoft rejected connection authorization", "A Microsoft recusou a autorização da conexão", "Microsoft a refusé l'autorisation de connexion", "Microsoft hat die Verbindungsfreigabe abgelehnt", "Microsoft ha rifiutato l'autorizzazione della connessione"},
    {"Microsoft rechazo la solicitud", "Microsoft rejected the request", "A Microsoft recusou a solicitação", "Microsoft a refusé la demande", "Microsoft hat die Anfrage abgelehnt", "Microsoft ha rifiutato la richiesta"},
    {"Microsoft rechazo los permisos pedidos", "Microsoft rejected the requested permissions", "A Microsoft recusou as permissões solicitadas", "Microsoft a refusé les autorisations demandées", "Microsoft hat die angeforderten Berechtigungen abgelehnt", "Microsoft ha rifiutato le autorizzazioni richieste"},
    {"Passport: acceso denegado (access_denied)", "Passport: access denied (access_denied)", "Passport: acesso negado (access_denied)", "Passport : accès refusé (access_denied)", "Passport: Zugriff verweigert (access_denied)", "Passport: accesso negato (access_denied)"},
    {"Passport: aplicacion no aceptada (invalid_client)", "Passport: app not accepted (invalid_client)", "Passport: aplicativo não aceito (invalid_client)", "Passport : application refusée (invalid_client)", "Passport: App abgelehnt (invalid_client)", "Passport: app rifiutata (invalid_client)"},
    {"Passport: aplicacion no autorizada (unauthorized_client)", "Passport: app not authorized (unauthorized_client)", "Passport: aplicativo não autorizado (unauthorized_client)", "Passport : application non autorisée (unauthorized_client)", "Passport: App nicht autorisiert (unauthorized_client)", "Passport: app non autorizzata (unauthorized_client)"},
    {"Passport: concesion no admitida (unsupported_grant_type)", "Passport: grant type not supported (unsupported_grant_type)", "Passport: concessão não aceita (unsupported_grant_type)", "Passport : type d'accès non accepté (unsupported_grant_type)", "Passport: Berechtigungstyp nicht unterstützt (unsupported_grant_type)", "Passport: tipo di concessione non supportato (unsupported_grant_type)"},
    {"Passport: error del servidor (server_error)", "Passport: server error (server_error)", "Passport: erro do servidor (server_error)", "Passport : erreur du serveur (server_error)", "Passport: Serverfehler (server_error)", "Passport: errore del server (server_error)"},
    {"Passport: permiso rechazado (invalid_scope)", "Passport: permission rejected (invalid_scope)", "Passport: permissão recusada (invalid_scope)", "Passport : autorisation refusée (invalid_scope)", "Passport: Berechtigung abgelehnt (invalid_scope)", "Passport: autorizzazione rifiutata (invalid_scope)"},
    {"Passport: renovacion no aceptada (invalid_grant)", "Passport: renewal rejected (invalid_grant)", "Passport: renovação recusada (invalid_grant)", "Passport : renouvellement refusé (invalid_grant)", "Passport: Erneuerung abgelehnt (invalid_grant)", "Passport: rinnovo rifiutato (invalid_grant)"},
    {"Passport: servicio no disponible (temporarily_unavailable)", "Passport: service unavailable (temporarily_unavailable)", "Passport: serviço indisponível (temporarily_unavailable)", "Passport : service indisponible (temporarily_unavailable)", "Passport: Dienst nicht verfügbar (temporarily_unavailable)", "Passport: servizio non disponibile (temporarily_unavailable)"},
    {"Passport: solicitud no valida (invalid_request)", "Passport: invalid request (invalid_request)", "Passport: solicitação inválida (invalid_request)", "Passport : demande invalide (invalid_request)", "Passport: Ungültige Anfrage (invalid_request)", "Passport: richiesta non valida (invalid_request)"},
    {"Passport: rechazo sin codigo conocido (unknown)", "Passport: rejection without known code (unknown)", "Passport: recusa sem código conhecido (unknown)", "Passport : refus sans code connu (unknown)", "Passport: Ablehnung ohne bekannten Code (unknown)", "Passport: rifiuto senza codice noto (unknown)"},
    {"TLS con Microsoft verificado", "Microsoft TLS verified", "TLS da Microsoft verificado", "TLS Microsoft vérifié", "Microsoft-TLS geprüft", "TLS Microsoft verificato"},
    {"URL o pagina de titulos invalida", "Invalid title URL or page", "URL ou página de jogos inválida", "URL ou page de jeux invalide", "Ungültige Spiele-URL oder -Seite", "URL o pagina dei giochi non valida"},
    {"WebRTC conectado sin recibir imagen o sonido", "WebRTC connected without video or audio", "WebRTC conectado sem vídeo ou áudio", "WebRTC connecté sans vidéo ni audio", "WebRTC verbunden, ohne Bild oder Ton", "WebRTC connesso senza video o audio"},
    {"WebRTC conectado, esperando imagen y sonido", "WebRTC connected; waiting for video and audio", "WebRTC conectado; aguardando vídeo e áudio", "WebRTC connecté ; attente de la vidéo et de l'audio", "WebRTC verbunden; warte auf Bild und Ton", "WebRTC connesso; attesa di video e audio"},
    {"WebRTC no conecto a tiempo", "WebRTC connection timed out", "A conexão WebRTC expirou", "La connexion WebRTC a expiré", "WebRTC-Verbindung abgelaufen", "Connessione WebRTC scaduta"},
    {"WebRTC no negocio a tiempo", "WebRTC negotiation timed out", "A negociação WebRTC expirou", "La négociation WebRTC a expiré", "WebRTC-Aushandlung abgelaufen", "Negoziazione WebRTC scaduta"},
    {"WebRTC rechazo la respuesta SDP", "WebRTC rejected the SDP answer", "WebRTC recusou a resposta SDP", "WebRTC a refusé la réponse SDP", "WebRTC hat die SDP-Antwort abgelehnt", "WebRTC ha rifiutato la risposta SDP"},
    {"WebRTC rechazo un candidato ICE", "WebRTC rejected an ICE candidate", "WebRTC recusou um candidato ICE", "WebRTC a refusé un candidat ICE", "WebRTC hat einen ICE-Kandidaten abgelehnt", "WebRTC ha rifiutato un candidato ICE"},
    {"Xbox Cloud Gaming rechazo la cuenta", "Xbox Cloud Gaming rejected the account", "Xbox Cloud Gaming recusou a conta", "Xbox Cloud Gaming a refusé le compte", "Xbox Cloud Gaming hat das Konto abgelehnt", "Xbox Cloud Gaming ha rifiutato l'account"},
    {"Xbox acepto la autorizacion de conexion", "Xbox accepted connection authorization", "Xbox aceitou a autorização da conexão", "Xbox a accepté l'autorisation de connexion", "Xbox hat die Verbindungsfreigabe akzeptiert", "Xbox ha accettato l'autorizzazione della connessione"},
    {"Xbox lista para negociar la conexion", "Xbox ready to negotiate the connection", "Xbox pronto para negociar a conexão", "Xbox prêt à négocier la connexion", "Xbox bereit zur Verbindungsaushandlung", "Xbox pronto a negoziare la connessione"},
    {"Xbox no acepto la oferta SDP", "Xbox did not accept the SDP offer", "Xbox não aceitou a oferta SDP", "Xbox n'a pas accepté l'offre SDP", "Xbox hat das SDP-Angebot nicht akzeptiert", "Xbox non ha accettato l'offerta SDP"},
    {"Xbox no acepto los candidatos ICE", "Xbox did not accept the ICE candidates", "Xbox não aceitou os candidatos ICE", "Xbox n'a pas accepté les candidats ICE", "Xbox hat die ICE-Kandidaten nicht akzeptiert", "Xbox non ha accettato i candidati ICE"},
    {"Xbox no aprovisiono la conexion a tiempo", "Xbox did not provision the connection in time", "Xbox não preparou a conexão a tempo", "Xbox n'a pas préparé la connexion à temps", "Xbox hat die Verbindung nicht rechtzeitig bereitgestellt", "Xbox non ha preparato la connessione in tempo"},
    {"Xbox no confirmo el estado de conexion", "Xbox did not confirm connection state", "Xbox não confirmou o estado da conexão", "Xbox n'a pas confirmé l'état de connexion", "Xbox hat den Verbindungsstatus nicht bestätigt", "Xbox non ha confermato lo stato della connessione"},
    {"Xbox no confirmo el mantenimiento de sesion", "Xbox did not confirm session keepalive", "Xbox não confirmou a manutenção da sessão", "Xbox n'a pas confirmé le maintien de session", "Xbox hat die Sitzungsverlängerung nicht bestätigt", "Xbox non ha confermato il mantenimento della sessione"},
    {"Xbox no esta disponible temporalmente. Reintenta.", "Xbox is temporarily unavailable. Try again.", "Xbox indisponível no momento. Tente de novo.", "Xbox est temporairement indisponible. Réessayez.", "Xbox ist vorübergehend nicht verfügbar. Erneut versuchen.", "Xbox non è disponibile al momento. Riprova."},
    {"Xbox no inicio la sesion", "Xbox did not start the session", "Xbox não iniciou a sessão", "Xbox n'a pas lancé la session", "Xbox hat die Sitzung nicht gestartet", "Xbox non ha avviato la sessione"},
    {"Xbox no mantuvo la sesion", "Xbox did not maintain the session", "Xbox não manteve a sessão", "Xbox n'a pas maintenu la session", "Xbox hat die Sitzung nicht aufrechterhalten", "Xbox non ha mantenuto la sessione"},
    {"Xbox no preparo la sesion a tiempo", "Xbox did not prepare the session in time", "Xbox não preparou a sessão a tempo", "Xbox n'a pas préparé la session à temps", "Xbox hat die Sitzung nicht rechtzeitig vorbereitet", "Xbox non ha preparato la sessione in tempo"},
    {"Xbox no respondio al SDP a tiempo", "Xbox did not answer SDP in time", "Xbox não respondeu ao SDP a tempo", "Xbox n'a pas répondu au SDP à temps", "Xbox hat nicht rechtzeitig auf SDP geantwortet", "Xbox non ha risposto all'SDP in tempo"},
    {"Xbox pide esperar antes de reintentar.", "Xbox asks you to wait before retrying.", "Xbox pede para aguardar antes de tentar de novo.", "Xbox demande d'attendre avant de réessayer.", "Xbox bittet, vor einem neuen Versuch zu warten.", "Xbox chiede di attendere prima di riprovare."},
    {"Xbox prepara la sesion", "Xbox is preparing the session", "Xbox está preparando a sessão", "Xbox prépare la session", "Xbox bereitet die Sitzung vor", "Xbox sta preparando la sessione"},
    {"Xbox rechazo el intercambio ICE", "Xbox rejected the ICE exchange", "Xbox recusou a troca ICE", "Xbox a refusé l'échange ICE", "Xbox hat den ICE-Austausch abgelehnt", "Xbox ha rifiutato lo scambio ICE"},
    {"Xbox rechazo el intercambio SDP", "Xbox rejected the SDP exchange", "Xbox recusou a troca SDP", "Xbox a refusé l'échange SDP", "Xbox hat den SDP-Austausch abgelehnt", "Xbox ha rifiutato lo scambio SDP"},
    {"Xbox rechazo la autorizacion XSTS", "Xbox rejected XSTS authorization", "Xbox recusou a autorização XSTS", "Xbox a refusé l'autorisation XSTS", "Xbox hat die XSTS-Autorisierung abgelehnt", "Xbox ha rifiutato l'autorizzazione XSTS"},
    {"Xbox rechazo la autorizacion de conexion", "Xbox rejected connection authorization", "Xbox recusou a autorização da conexão", "Xbox a refusé l'autorisation de connexion", "Xbox hat die Verbindungsfreigabe abgelehnt", "Xbox ha rifiutato l'autorizzazione della connessione"},
    {"Xbox rechazo la conexion aprovisionada", "Xbox rejected the provisioned connection", "Xbox recusou a conexão preparada", "Xbox a refusé la connexion préparée", "Xbox hat die bereitgestellte Verbindung abgelehnt", "Xbox ha rifiutato la connessione preparata"},
    {"Xbox rechazo la cuenta Microsoft", "Xbox rejected the Microsoft account", "Xbox recusou a conta Microsoft", "Xbox a refusé le compte Microsoft", "Xbox hat das Microsoft-Konto abgelehnt", "Xbox ha rifiutato l'account Microsoft"},
    {"Xbox rechazo la lista de titulos", "Xbox rejected the game list", "Xbox recusou a lista de jogos", "Xbox a refusé la liste des jeux", "Xbox hat die Spieleliste abgelehnt", "Xbox ha rifiutato l'elenco dei giochi"},
    {"Xbox rechazo la negociacion SDP", "Xbox rejected SDP negotiation", "Xbox recusou a negociação SDP", "Xbox a refusé la négociation SDP", "Xbox hat die SDP-Aushandlung abgelehnt", "Xbox ha rifiutato la negoziazione SDP"},
    {"Xbox reporto un fallo de la sesion", "Xbox reported a session failure", "Xbox informou falha na sessão", "Xbox a signalé un échec de session", "Xbox hat einen Sitzungsfehler gemeldet", "Xbox ha segnalato un errore di sessione"},
    {"Xbox termino la sesion", "Xbox ended the session", "Xbox encerrou a sessão", "Xbox a terminé la session", "Xbox hat die Sitzung beendet", "Xbox ha terminato la sessione"},
    {"acceso caducado; X reintenta", "Access expired; press X to retry", "Acesso expirado; pressione X para tentar de novo", "Accès expiré ; appuyez sur X pour réessayer", "Zugriff abgelaufen; X zum Wiederholen drücken", "Accesso scaduto; premi X per riprovare"},
    {"acceso cancelado", "Access cancelled", "Acesso cancelado", "Accès annulé", "Zugriff abgebrochen", "Accesso annullato"},
    {"acceso rechazado en la cuenta", "Access denied for this account", "Acesso negado para esta conta", "Accès refusé pour ce compte", "Zugriff für dieses Konto verweigert", "Accesso negato per questo account"},
    {"autorizacion XSTS", "XSTS authorization", "Autorização XSTS", "Autorisation XSTS", "XSTS-Autorisierung", "Autorizzazione XSTS"},
    {"autorizacion cancelada", "Authorization cancelled", "Autorização cancelada", "Autorisation annulée", "Autorisierung abgebrochen", "Autorizzazione annullata"},
    {"autorizacion de conexion no disponible", "Connection authorization unavailable", "Autorização da conexão indisponível", "Autorisation de connexion indisponible", "Verbindungsfreigabe nicht verfügbar", "Autorizzazione della connessione non disponibile"},
    {"autorizacion de conexion no valida", "Invalid connection authorization", "Autorização da conexão inválida", "Autorisation de connexion invalide", "Ungültige Verbindungsfreigabe", "Autorizzazione della connessione non valida"},
    {"autorizando conexion con Microsoft", "Authorizing connection with Microsoft", "Autorizando conexão com a Microsoft", "Autorisation de la connexion avec Microsoft", "Verbindung mit Microsoft wird autorisiert", "Autorizzazione della connessione con Microsoft"},
    {"cancelando...", "Cancelling...", "Cancelando...", "Annulation...", "Abbrechen...", "Annullamento..."},
    {"candidato ICE fuera de limite", "ICE candidate exceeds limit", "Candidato ICE excede o limite", "Candidat ICE hors limite", "ICE-Kandidat überschreitet das Limit", "Candidato ICE oltre il limite"},
    {"candidato ICE remoto no valido", "Invalid remote ICE candidate", "Candidato ICE remoto inválido", "Candidat ICE distant invalide", "Ungültiger entfernter ICE-Kandidat", "Candidato ICE remoto non valido"},
    {"catalogo listo", "Game catalog ready", "Catálogo de jogos pronto", "Catalogue de jeux prêt", "Spielekatalog bereit", "Catalogo giochi pronto"},
    {"cerrando sesion en Xbox", "Closing Xbox session", "Encerrando a sessão Xbox", "Fermeture de la session Xbox", "Xbox-Sitzung wird geschlossen", "Chiusura della sessione Xbox"},
    {"cerrando sesion...", "Closing session...", "Encerrando sessão...", "Fermeture de la session...", "Sitzung wird geschlossen...", "Chiusura della sessione..."},
    {"comprobando conexion TLS", "Checking TLS connection", "Verificando conexão TLS", "Vérification de la connexion TLS", "TLS-Verbindung wird geprüft", "Verifica della connessione TLS"},
    {"conectando con Microsoft", "Connecting to Microsoft", "Conectando à Microsoft", "Connexion à Microsoft", "Verbindung zu Microsoft", "Connessione a Microsoft"},
    {"consulta cancelada", "Request cancelled", "Consulta cancelada", "Requête annulée", "Anfrage abgebrochen", "Richiesta annullata"},
    {"consulta de conexion no valida", "Invalid connection status request", "Consulta do estado da conexão inválida", "Demande d'état de connexion invalide", "Ungültige Verbindungsstatusanfrage", "Richiesta dello stato della connessione non valida"},
    {"consulta de estado", "Checking status", "Consultando estado", "Vérification de l'état", "Status wird geprüft", "Verifica dello stato"},
    {"credencial de region", "Region credential", "Credencial da região", "Identifiant de région", "Regionsnachweis", "Credenziale della regione"},
    {"credenciales de Xbox Cloud Gaming", "Xbox Cloud Gaming credentials", "Credenciais do Xbox Cloud Gaming", "Identifiants Xbox Cloud Gaming", "Xbox-Cloud-Gaming-Anmeldedaten", "Credenziali Xbox Cloud Gaming"},
    {"credenciales de juegos gratuitos", "Free game credentials", "Credenciais para jogos grátis", "Identifiants pour jeux gratuits", "Anmeldedaten für kostenlose Spiele", "Credenziali per giochi gratuiti"},
    {"credenciales no validas", "Invalid credentials", "Credenciais inválidas", "Identifiants invalides", "Ungültige Anmeldedaten", "Credenziali non valide"},
    {"cuenta Microsoft autorizada", "Microsoft account authorized", "Conta Microsoft autorizada", "Compte Microsoft autorisé", "Microsoft-Konto autorisiert", "Account Microsoft autorizzato"},
    {"cuenta Microsoft autorizada y guardada", "Microsoft account authorized and saved", "Conta Microsoft autorizada e salva", "Compte Microsoft autorisé et enregistré", "Microsoft-Konto autorisiert und gespeichert", "Account Microsoft autorizzato e salvato"},
    {"cuenta Microsoft recuperada", "Microsoft account restored", "Conta Microsoft recuperada", "Compte Microsoft récupéré", "Microsoft-Konto wiederhergestellt", "Account Microsoft recuperato"},
    {"cuenta activa; acceso no guardado", "Account active; access not saved", "Conta ativa; acesso não salvo", "Compte actif ; accès non enregistré", "Konto aktiv; Zugriff nicht gespeichert", "Account attivo; accesso non salvato"},
    {"cuenta autorizada; acceso no guardado", "Account authorized; access not saved", "Conta autorizada; acesso não salvo", "Compte autorisé ; accès non enregistré", "Konto autorisiert; Zugriff nicht gespeichert", "Account autorizzato; accesso non salvato"},
    {"cuenta recuperada; acceso no guardado", "Account restored; access not saved", "Conta recuperada; acesso não salvo", "Compte récupéré ; accès non enregistré", "Konto wiederhergestellt; Zugriff nicht gespeichert", "Account recuperato; accesso non salvato"},
    {"demasiados candidatos ICE locales", "Too many local ICE candidates", "Candidatos ICE locais demais", "Trop de candidats ICE locaux", "Zu viele lokale ICE-Kandidaten", "Troppi candidati ICE locali"},
    {"demasiados candidatos ICE remotos", "Too many remote ICE candidates", "Candidatos ICE remotos demais", "Trop de candidats ICE distants", "Zu viele entfernte ICE-Kandidaten", "Troppi candidati ICE remoti"},
    {"el codigo caduco", "The code expired", "O código expirou", "Le code a expiré", "Der Code ist abgelaufen", "Il codice è scaduto"},
    {"enviando autorizacion a Xbox", "Sending authorization to Xbox", "Enviando autorização ao Xbox", "Envoi de l'autorisation à Xbox", "Autorisierung wird an Xbox gesendet", "Invio dell'autorizzazione a Xbox"},
    {"esperando autorizacion", "Waiting for authorization", "Aguardando autorização", "En attente d'autorisation", "Warte auf Autorisierung", "In attesa di autorizzazione"},
    {"esperando cierre seguro del control", "Waiting for safe control shutdown", "Aguardando encerramento seguro do controle", "Attente de fermeture sûre du contrôle", "Warte auf sicheres Schließen der Steuerung", "In attesa della chiusura sicura del controllo"},
    {"esperando conexion aprovisionada por Xbox", "Waiting for Xbox to prepare the connection", "Aguardando o Xbox preparar a conexão", "Attente de la connexion préparée par Xbox", "Warte auf die von Xbox bereitgestellte Verbindung", "In attesa della connessione preparata da Xbox"},
    {"esperando recursos de Xbox", "Waiting for Xbox resources", "Aguardando recursos do Xbox", "En attente des ressources Xbox", "Warte auf Xbox-Ressourcen", "In attesa delle risorse Xbox"},
    {"esperando respuesta SDP de Xbox", "Waiting for Xbox SDP answer", "Aguardando resposta SDP do Xbox", "En attente de la réponse SDP Xbox", "Warte auf Xbox-SDP-Antwort", "In attesa della risposta SDP di Xbox"},
    {"esperando respuesta de Xbox", "Waiting for Xbox response", "Aguardando resposta do Xbox", "En attente de la réponse Xbox", "Warte auf Xbox-Antwort", "In attesa della risposta Xbox"},
    {"estado HTTP inesperado", "Unexpected HTTP status", "Estado HTTP inesperado", "État HTTP inattendu", "Unerwarteter HTTP-Status", "Stato HTTP inatteso"},
    {"estado HTTP inesperado de la sesion", "Unexpected session HTTP status", "Estado HTTP inesperado da sessão", "État HTTP inattendu de la session", "Unerwarteter HTTP-Status der Sitzung", "Stato HTTP inatteso della sessione"},
    {"estado HTTP inesperado en XSTS", "Unexpected HTTP status from XSTS", "Estado HTTP inesperado no XSTS", "État HTTP inattendu de XSTS", "Unerwarteter HTTP-Status von XSTS", "Stato HTTP inatteso da XSTS"},
    {"estado HTTP inesperado en credenciales", "Unexpected credential HTTP status", "Estado HTTP inesperado nas credenciais", "État HTTP inattendu des identifiants", "Unerwarteter HTTP-Status bei Anmeldedaten", "Stato HTTP inatteso per le credenziali"},
    {"estado de conexion no valido", "Invalid connection state", "Estado da conexão inválido", "État de connexion invalide", "Ungültiger Verbindungsstatus", "Stato della connessione non valido"},
    {"estado de sesion no valido", "Invalid session state", "Estado da sessão inválido", "État de session invalide", "Ungültiger Sitzungsstatus", "Stato della sessione non valido"},
    {"fallo la conexion WebRTC", "WebRTC connection failed", "Falha na conexão WebRTC", "Échec de la connexion WebRTC", "WebRTC-Verbindung fehlgeschlagen", "Connessione WebRTC non riuscita"},
    {"formulario de autorizacion de conexion", "Connection authorization form", "Formulário de autorização da conexão", "Formulaire d'autorisation de connexion", "Formular für Verbindungsfreigabe", "Modulo di autorizzazione della connessione"},
    {"formulario de codigo", "Code form", "Formulário de código", "Formulaire de code", "Codeformular", "Modulo del codice"},
    {"formulario de token", "Token form", "Formulário de token", "Formulaire de jeton", "Tokenformular", "Modulo del token"},
    {"iniciando HTTPS", "Starting HTTPS", "Iniciando HTTPS", "Démarrage HTTPS", "HTTPS wird gestartet", "Avvio di HTTPS"},
    {"inicio HTTPS (red/TLS)", "Starting HTTPS (network/TLS)", "Iniciando HTTPS (rede/TLS)", "Démarrage HTTPS (réseau/TLS)", "HTTPS-Start (Netzwerk/TLS)", "Avvio HTTPS (rete/TLS)"},
    {"la sesion de Microsoft caduco", "Microsoft session expired", "A sessão Microsoft expirou", "La session Microsoft a expiré", "Microsoft-Sitzung abgelaufen", "La sessione Microsoft è scaduta"},
    {"lista de titulos de la region", "Regional game list", "Lista regional de jogos", "Liste régionale des jeux", "Regionale Spieleliste", "Elenco regionale dei giochi"},
    {"lista de titulos incompleta", "Incomplete game list", "Lista de jogos incompleta", "Liste de jeux incomplète", "Unvollständige Spieleliste", "Elenco giochi incompleto"},
    {"metadatos de Microsoft no validos", "Invalid Microsoft metadata", "Metadados da Microsoft inválidos", "Métadonnées Microsoft invalides", "Ungültige Microsoft-Metadaten", "Metadati Microsoft non validi"},
    {"negociando ruta ICE con Xbox", "Negotiating ICE route with Xbox", "Negociando rota ICE com Xbox", "Négociation de la route ICE avec Xbox", "ICE-Route mit Xbox wird ausgehandelt", "Negoziazione del percorso ICE con Xbox"},
    {"no se confirmo el cierre de la sesion", "Session closure was not confirmed", "O encerramento da sessão não foi confirmado", "La fermeture de session n'a pas été confirmée", "Schließen der Sitzung nicht bestätigt", "Chiusura della sessione non confermata"},
    {"no se pudo borrar el acceso guardado", "Could not delete saved access", "Não foi possível apagar o acesso salvo", "Impossible de supprimer l'accès enregistré", "Gespeicherter Zugriff konnte nicht gelöscht werden", "Impossibile eliminare l'accesso salvato"},
    {"no se pudo iniciar WebRTC nativo", "Could not start native WebRTC", "Não foi possível iniciar o WebRTC nativo", "Impossible de lancer WebRTC natif", "Natives WebRTC konnte nicht gestartet werden", "Impossibile avviare WebRTC nativo"},
    {"no se pudo iniciar el acceso", "Could not start sign-in", "Não foi possível iniciar o acesso", "Impossible de lancer la connexion", "Anmeldung konnte nicht gestartet werden", "Impossibile avviare l'accesso"},
    {"no se pudo iniciar envio de controles", "Could not start controller input", "Não foi possível iniciar os controles", "Impossible de lancer les commandes", "Controller-Eingabe konnte nicht gestartet werden", "Impossibile avviare i comandi"},
    {"no se pudo iniciar la consulta", "Could not start the request", "Não foi possível iniciar a consulta", "Impossible de lancer la requête", "Anfrage konnte nicht gestartet werden", "Impossibile avviare la richiesta"},
    {"no se pudo iniciar la sesion", "Could not start the session", "Não foi possível iniciar a sessão", "Impossible de lancer la session", "Sitzung konnte nicht gestartet werden", "Impossibile avviare la sessione"},
    {"no se pudo preparar la oferta WebRTC", "Could not prepare the WebRTC offer", "Não foi possível preparar a oferta WebRTC", "Impossible de préparer l'offre WebRTC", "WebRTC-Angebot konnte nicht vorbereitet werden", "Impossibile preparare l'offerta WebRTC"},
    {"no se pudo recuperar la cuenta", "Could not restore the account", "Não foi possível recuperar a conta", "Impossible de récupérer le compte", "Konto konnte nicht wiederhergestellt werden", "Impossibile recuperare l'account"},
    {"no se pudo reunir candidatos ICE", "Could not gather ICE candidates", "Não foi possível reunir candidatos ICE", "Impossible de réunir les candidats ICE", "ICE-Kandidaten konnten nicht gesammelt werden", "Impossibile raccogliere candidati ICE"},
    {"nombres de Microsoft Store", "Microsoft Store game names", "Nomes dos jogos na Microsoft Store", "Noms des jeux Microsoft Store", "Spielenamen aus dem Microsoft Store", "Nomi dei giochi nel Microsoft Store"},
    {"oferta WebRTC fuera de limite", "WebRTC offer exceeds limit", "Oferta WebRTC excede o limite", "Offre WebRTC hors limite", "WebRTC-Angebot überschreitet das Limit", "Offerta WebRTC oltre il limite"},
    {"oferta WebRTC no disponible", "WebRTC offer unavailable", "Oferta WebRTC indisponível", "Offre WebRTC indisponible", "WebRTC-Angebot nicht verfügbar", "Offerta WebRTC non disponibile"},
    {"pagina de verificacion desconocida", "Unknown verification page", "Página de verificação desconhecida", "Page de vérification inconnue", "Unbekannte Bestätigungsseite", "Pagina di verifica sconosciuta"},
    {"preparando consulta Xbox", "Preparing Xbox request", "Preparando consulta ao Xbox", "Préparation de la requête Xbox", "Xbox-Anfrage wird vorbereitet", "Preparazione della richiesta Xbox"},
    {"preparando oferta WebRTC", "Preparing WebRTC offer", "Preparando oferta WebRTC", "Préparation de l'offre WebRTC", "WebRTC-Angebot wird vorbereitet", "Preparazione dell'offerta WebRTC"},
    {"preparando sesion Xbox", "Preparing Xbox session", "Preparando sessão Xbox", "Préparation de la session Xbox", "Xbox-Sitzung wird vorbereitet", "Preparazione della sessione Xbox"},
    {"recibiendo audio de Xbox; esperando video", "Receiving Xbox audio; waiting for video", "Recebendo áudio do Xbox; aguardando vídeo", "Audio Xbox reçu ; attente de la vidéo", "Xbox-Ton empfangen; warte auf Bild", "Ricezione audio Xbox; attesa del video"},
    {"recibiendo imagen y sonido de Xbox", "Receiving Xbox video and audio", "Recebendo vídeo e áudio do Xbox", "Vidéo et audio Xbox reçus", "Xbox-Bild und -Ton empfangen", "Ricezione video e audio Xbox"},
    {"recibiendo video de Xbox; esperando audio", "Receiving Xbox video; waiting for audio", "Recebendo vídeo do Xbox; aguardando áudio", "Vidéo Xbox reçue ; attente de l'audio", "Xbox-Bild empfangen; warte auf Ton", "Ricezione video Xbox; attesa dell'audio"},
    {"recuperando cuenta Microsoft", "Restoring Microsoft account", "Recuperando conta Microsoft", "Récupération du compte Microsoft", "Microsoft-Konto wird wiederhergestellt", "Recupero dell'account Microsoft"},
    {"region predeterminada no valida", "Invalid default region", "Região padrão inválida", "Région par défaut invalide", "Ungültige Standardregion", "Regione predefinita non valida"},
    {"renovacion de Microsoft no valida", "Invalid Microsoft renewal", "Renovação da Microsoft inválida", "Renouvellement Microsoft invalide", "Ungültige Microsoft-Erneuerung", "Rinnovo Microsoft non valido"},
    {"respuesta ICE no valida", "Invalid ICE answer", "Resposta ICE inválida", "Réponse ICE invalide", "Ungültige ICE-Antwort", "Risposta ICE non valida"},
    {"respuesta SDP no valida", "Invalid SDP answer", "Resposta SDP inválida", "Réponse SDP invalide", "Ungültige SDP-Antwort", "Risposta SDP non valida"},
    {"respuesta de autorizacion no valida", "Invalid authorization response", "Resposta de autorização inválida", "Réponse d'autorisation invalide", "Ungültige Autorisierungsantwort", "Risposta di autorizzazione non valida"},
    {"respuesta de codigo no valida", "Invalid code response", "Resposta de código inválida", "Réponse de code invalide", "Ungültige Codeantwort", "Risposta del codice non valida"},
    {"respuesta de error no valida", "Invalid error response", "Resposta de erro inválida", "Réponse d'erreur invalide", "Ungültige Fehlerantwort", "Risposta di errore non valida"},
    {"respuesta de sesion no valida", "Invalid session response", "Resposta de sessão inválida", "Réponse de session invalide", "Ungültige Sitzungsantwort", "Risposta della sessione non valida"},
    {"respuesta de token no valida", "Invalid token response", "Resposta de token inválida", "Réponse de jeton invalide", "Ungültige Tokenantwort", "Risposta del token non valida"},
    {"se perdio la conexion WebRTC", "WebRTC connection lost", "Conexão WebRTC perdida", "Connexion WebRTC perdue", "WebRTC-Verbindung verloren", "Connessione WebRTC persa"},
    {"sesion aprovisionada por Xbox", "Session prepared by Xbox", "Sessão preparada pelo Xbox", "Session préparée par Xbox", "Sitzung von Xbox bereitgestellt", "Sessione preparata da Xbox"},
    {"sesion cancelada", "Session cancelled", "Sessão cancelada", "Session annulée", "Sitzung abgebrochen", "Sessione annullata"},
    {"sin catalogo", "No catalog", "Sem catálogo", "Aucun catalogue", "Kein Katalog", "Nessun catalogo"},
    {"sin cuenta Microsoft", "No Microsoft account", "Sem conta Microsoft", "Aucun compte Microsoft", "Kein Microsoft-Konto", "Nessun account Microsoft"},
    {"sin memoria para autorizar la conexion", "Not enough memory to authorize the connection", "Memória insuficiente para autorizar a conexão", "Mémoire insuffisante pour autoriser la connexion", "Zu wenig Speicher für die Verbindungsfreigabe", "Memoria insufficiente per autorizzare la connessione"},
    {"sin memoria para el acceso", "Not enough memory for sign-in", "Memória insuficiente para o acesso", "Mémoire insuffisante pour la connexion", "Zu wenig Speicher für die Anmeldung", "Memoria insufficiente per l'accesso"},
    {"sin memoria para el catalogo", "Not enough memory for the catalog", "Memória insuficiente para o catálogo", "Mémoire insuffisante pour le catalogue", "Zu wenig Speicher für den Katalog", "Memoria insufficiente per il catalogo"},
    {"sin memoria para la sesion", "Not enough memory for the session", "Memória insuficiente para a sessão", "Mémoire insuffisante pour la session", "Zu wenig Speicher für die Sitzung", "Memoria insufficiente per la sessione"},
    {"sin memoria para recuperar la cuenta", "Not enough memory to restore the account", "Memória insuficiente para recuperar a conta", "Mémoire insuffisante pour récupérer le compte", "Zu wenig Speicher zur Kontowiederherstellung", "Memoria insufficiente per recuperare l'account"},
    {"sin sesion", "No session", "Sem sessão", "Aucune session", "Keine Sitzung", "Nessuna sessione"},
    {"solicitando codigo a Microsoft", "Requesting code from Microsoft", "Solicitando código à Microsoft", "Demande de code à Microsoft", "Code wird bei Microsoft angefordert", "Richiesta del codice a Microsoft"},
    {"solicitando sesion en la nube", "Requesting cloud session", "Solicitando sessão na nuvem", "Demande de session cloud", "Cloud-Sitzung wird angefordert", "Richiesta della sessione cloud"},
    {"solicitud ICE fuera de limite", "ICE request exceeds limit", "Solicitação ICE excede o limite", "Demande ICE hors limite", "ICE-Anfrage überschreitet das Limit", "Richiesta ICE oltre il limite"},
    {"solicitud WebRTC no valida", "Invalid WebRTC request", "Solicitação WebRTC inválida", "Demande WebRTC invalide", "Ungültige WebRTC-Anfrage", "Richiesta WebRTC non valida"},
    {"solicitud XSTS", "XSTS request", "Solicitação XSTS", "Demande XSTS", "XSTS-Anfrage", "Richiesta XSTS"},
    {"solicitud de autorizacion de conexion", "Connection authorization request", "Solicitação de autorização da conexão", "Demande d'autorisation de connexion", "Anfrage zur Verbindungsfreigabe", "Richiesta di autorizzazione della connessione"},
    {"solicitud de credenciales", "Credential request", "Solicitação de credenciais", "Demande d'identifiants", "Anmeldedatenanfrage", "Richiesta di credenziali"},
    {"solicitud de sesion", "Session request", "Solicitação de sessão", "Demande de session", "Sitzungsanfrage", "Richiesta di sessione"},
    {"solicitud de token Xbox", "Xbox token request", "Solicitação de token Xbox", "Demande de jeton Xbox", "Xbox-Tokenanfrage", "Richiesta di token Xbox"},
    {"titulo u oferta no valida", "Invalid title or offering", "Jogo ou oferta inválida", "Jeu ou offre invalide", "Ungültiges Spiel oder Angebot", "Titolo o offerta non validi"},
    {"token Microsoft caducado", "Microsoft token expired", "Token Microsoft expirado", "Jeton Microsoft expiré", "Microsoft-Token abgelaufen", "Token Microsoft scaduto"},
    {"token XSTS no valido", "Invalid XSTS token", "Token XSTS inválido", "Jeton XSTS invalide", "Ungültiges XSTS-Token", "Token XSTS non valido"},
    {"token Xbox no valido", "Invalid Xbox token", "Token Xbox inválido", "Jeton Xbox invalide", "Ungültiges Xbox-Token", "Token Xbox non valido"},
    {"token de usuario Xbox", "Xbox user token", "Token de usuário Xbox", "Jeton utilisateur Xbox", "Xbox-Benutzertoken", "Token utente Xbox"},
    {"verificando token", "Checking token", "Verificando token", "Vérification du jeton", "Token wird geprüft", "Verifica del token"},
};
/* x4_http_stage() is joined to a status as "HTTPS: <stage>" on errors. */
static const Entry http_entries[] = {
    {"modulos de red", "network modules", "módulos de rede", "modules réseau", "Netzwerkmodule", "moduli di rete"},
    {"NetInit", "NetInit", "NetInit", "NetInit", "NetInit", "NetInit"},
    {"pool de red", "network pool", "pool de rede", "réserve réseau", "Netzwerkpool", "pool di rete"},
    {"SslInit", "SslInit", "SslInit", "SslInit", "SslInit", "SslInit"},
    {"HttpInit", "HttpInit", "HttpInit", "HttpInit", "HttpInit", "HttpInit"},
    {"HTTPS listo", "HTTPS ready", "HTTPS pronto", "HTTPS prêt", "HTTPS bereit", "HTTPS pronto"},
    {"plantilla HTTPS", "HTTPS template", "modelo HTTPS", "modèle HTTPS", "HTTPS-Vorlage", "modello HTTPS"},
    {"tiempo de resolucion", "resolution timeout", "prazo de resolução", "délai de résolution", "Auflösungsfrist", "tempo di risoluzione"},
    {"tiempo de conexion", "connection timeout", "prazo de conexão", "délai de connexion", "Verbindungsfrist", "tempo di connessione"},
    {"tiempo de envio", "send timeout", "prazo de envio", "délai d'envoi", "Sendefrist", "tempo di invio"},
    {"tiempo de recepcion", "receive timeout", "prazo de recebimento", "délai de réception", "Empfangsfrist", "tempo di ricezione"},
    {"redirecciones", "redirects", "redirecionamentos", "redirections", "Weiterleitungen", "reindirizzamenti"},
    {"cookies", "cookies", "cookies", "cookies", "Cookies", "cookie"},
    {"verificacion TLS", "TLS verification", "verificação TLS", "vérification TLS", "TLS-Prüfung", "verifica TLS"},
    {"conexion HTTPS", "HTTPS connection", "conexão HTTPS", "connexion HTTPS", "HTTPS-Verbindung", "connessione HTTPS"},
    {"solicitud HTTPS", "HTTPS request", "solicitação HTTPS", "requête HTTPS", "HTTPS-Anfrage", "richiesta HTTPS"},
    {"cabeceras", "headers", "cabeçalhos", "en-têtes", "Header", "intestazioni"},
    {"envio HTTPS/TLS", "HTTPS/TLS send", "envio HTTPS/TLS", "envoi HTTPS/TLS", "HTTPS/TLS-Senden", "invio HTTPS/TLS"},
    {"codigo de estado", "status code", "código de estado", "code d'état", "Statuscode", "codice di stato"},
    {"lectura de respuesta", "reading response", "leitura da resposta", "lecture de la réponse", "Antwort wird gelesen", "lettura della risposta"},
    {"respuesta excede bufer", "response exceeds buffer", "resposta excede o buffer", "réponse trop volumineuse", "Antwort überschreitet den Puffer", "risposta oltre il buffer"},
    {"memoria de respuesta", "response memory", "memória da resposta", "mémoire de réponse", "Antwortspeicher", "memoria della risposta"},
    {"respuesta comprimida invalida", "invalid compressed response", "resposta compactada inválida", "réponse compressée invalide", "Ungültige komprimierte Antwort", "risposta compressa non valida"},
    {"cancelado", "cancelled", "cancelado", "annulé", "abgebrochen", "annullato"},
    {"plazo agotado", "deadline exceeded", "prazo esgotado", "délai dépassé", "Frist überschritten", "tempo scaduto"},
    {"respuesta recibida", "response received", "resposta recebida", "réponse reçue", "Antwort empfangen", "risposta ricevuta"},
    {"recursos HTTP en cuarentena", "HTTP resources quarantined", "recursos HTTP em quarentena", "ressources HTTP isolées", "HTTP-Ressourcen isoliert", "risorse HTTP isolate"},
    {"argumentos HTTPS", "HTTPS arguments", "argumentos HTTPS", "arguments HTTPS", "HTTPS-Argumente", "argomenti HTTPS"},
    {"HTTPS inutilizable", "HTTPS unusable", "HTTPS inutilizável", "HTTPS inutilisable", "HTTPS unbrauchbar", "HTTPS inutilizzabile"},
    {"URL rechazada", "URL rejected", "URL recusada", "URL refusée", "URL abgelehnt", "URL rifiutato"},
    {"formulario rechazado", "form rejected", "formulário recusado", "formulaire refusé", "Formular abgelehnt", "modulo rifiutato"},
    {"memoria de solicitud", "request memory", "memória da solicitação", "mémoire de requête", "Anfragespeicher", "memoria della richiesta"},
    {"metodo rechazado", "method rejected", "método recusado", "méthode refusée", "Methode abgelehnt", "metodo rifiutato"},
    {"cuerpo JSON rechazado", "JSON body rejected", "corpo JSON recusado", "corps JSON refusé", "JSON-Inhalt abgelehnt", "corpo JSON rifiutato"},
    {"cabecera rechazada", "header rejected", "cabeçalho recusado", "en-tête refusé", "Header abgelehnt", "intestazione rifiutata"},
};

size_t x4_status_key_count(void) { return sizeof(entries) / sizeof(entries[0]); }
const char *x4_status_key(size_t index)
{ return index < x4_status_key_count() ? entries[index].key : NULL; }
static const char *translation(const Entry *e, int language)
{
    return language == X4_STATUS_EN ? e->en :
        language == X4_STATUS_PT_BR ? e->pt :
        language == X4_STATUS_FR ? e->fr :
        language == X4_STATUS_DE ? e->de :
        language == X4_STATUS_IT ? e->it : e->key;
}

static void capitalized_copy(char *dst, size_t capacity, const char *src)
{
    if (!capacity) return;
    size_t used = 0;
    while (src && src[used] && used + 1 < capacity) {
        unsigned char first = (unsigned char)src[used];
        size_t bytes = first < 0x80 ? 1u : first >= 0xc2 && first < 0xe0 ? 2u :
            first >= 0xe0 && first < 0xf0 ? 3u : first >= 0xf0 && first < 0xf5 ? 4u : 1u;
        if (used + bytes >= capacity) break;
        size_t i = 0;
        for (; i < bytes && src[used + i]; ++i) dst[used + i] = src[used + i];
        if (i != bytes) break;
        used += bytes;
    }
    dst[used] = 0;
    if (dst[0] >= 'a' && dst[0] <= 'z') { dst[0] -= 'a' - 'A'; return; }
    if ((unsigned char)dst[0] != 0xc3 || !dst[1]) return;
    switch ((unsigned char)dst[1]) {
    case 0xa1: dst[1] = (char)0x81; break; /* á → Á */
    case 0xa9: dst[1] = (char)0x89; break; /* é → É */
    case 0xad: dst[1] = (char)0x8d; break; /* í → Í */
    case 0xb3: dst[1] = (char)0x93; break; /* ó → Ó */
    case 0xba: dst[1] = (char)0x9a; break; /* ú → Ú */
    case 0xb1: dst[1] = (char)0x91; break; /* ñ → Ñ */
    case 0xbc: dst[1] = (char)0x9c; break; /* ü → Ü */
    default: break;
    }
}
void x4_status_format(char *dst, size_t capacity, const char *key, int language)
{
    if (!dst || !capacity) return;
    dst[0] = 0;
    if (!key) return;
    if (language < 0 || language >= X4_STATUS_LANGUAGES) language = X4_STATUS_EN;
    const char *selected = key;
    char https_status[256];
    for (size_t i = 0; i < x4_status_key_count(); ++i) {
        if (strcmp(entries[i].key, key)) continue;
        selected = translation(&entries[i], language);
        break;
    }
    if (selected == key && !strncmp(key, "HTTPS: ", 7)) {
        const char *suffix = key + 7;
        for (size_t i = 0; i < sizeof(http_entries) / sizeof(http_entries[0]); ++i) {
            if (strcmp(http_entries[i].key, suffix)) continue;
            memcpy(https_status, "HTTPS: ", 7);
            capitalized_copy(https_status + 7, sizeof(https_status) - 7,
                translation(&http_entries[i], language));
            selected = https_status;
            break;
        }
    }
    capitalized_copy(dst, capacity, selected);
}
