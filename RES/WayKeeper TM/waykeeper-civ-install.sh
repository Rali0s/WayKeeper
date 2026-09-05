#!/usr/bin/env bash
#
# WAYKEEPER(TM) CIV-INSTALL
# A dependency-free ANSI OOBE for founding a post-event encampment.
#
# Run:
#   bash waykeeper-civ-install.sh
#   bash waykeeper-civ-install.sh --target ./my-encampment
#
# Non-interactive:
#   WAYKEEPER_STEWARD="Name" WAYKEEPER_WITNESS="Name" \
#     bash waykeeper-civ-install.sh --defaults --target ./my-encampment

set -uo pipefail
umask 027

VERSION="0.7.7"
TARGET="./waykeeper-encampment"
USE_COLOR=1
DEFAULTS_MODE=0
MENU_RESULT=""

CENSUS="Census first"
SANCTUARY="Sanctuary + assembly"
DOCTRINE="Hybrid covenant"
GOVERNANCE="30-day incident command"
BUNDLE="Survival core"
LEDGERS="Dual paper + terminal"
SIGNATURE_METHOD="Names + witnesses"
WATCH="Community watch"
SALOON="Managed tavern"
STEWARD="${WAYKEEPER_STEWARD:-}"
WITNESS="${WAYKEEPER_WITNESS:-}"

CSI=""
GREEN=""
AMBER=""
RED=""
DIM=""
BOLD=""
SELECT=""
RESET=""

usage() {
  printf '%s\n' \
    "WAYKEEPER(TM) CIV-INSTALL v$VERSION" \
    "" \
    "Usage: $0 [options]" \
    "" \
    "Options:" \
    "  --target DIR   Create the encampment tree at DIR." \
    "  --defaults     Use recommended choices; signatures come from" \
    "                 WAYKEEPER_STEWARD and WAYKEEPER_WITNESS." \
    "  --no-color     Disable ANSI colors." \
    "  -h, --help     Show this help." \
    "" \
    "Safety: the target must not already exist. This installer never deletes" \
    "or overwrites an existing settlement directory."
}

while (( $# > 0 )); do
  case "$1" in
    --target)
      if (( $# < 2 )); then
        printf 'Missing value for --target\n' >&2
        exit 2
      fi
      TARGET="$2"
      shift 2
      ;;
    --defaults)
      DEFAULTS_MODE=1
      shift
      ;;
    --no-color)
      USE_COLOR=0
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      printf 'Unknown option: %s\n' "$1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

case "$TARGET" in
  ""|"/"|"."|".."|"${HOME:-}"|"${HOME:-}/")
    printf 'Refusing unsafe target: %s\n' "$TARGET" >&2
    exit 2
    ;;
esac

if [[ "$USE_COLOR" -eq 1 && -t 1 && "${TERM:-dumb}" != "dumb" ]]; then
  CSI=$'\033['
  GREEN="${CSI}38;5;46m"
  AMBER="${CSI}38;5;214m"
  RED="${CSI}38;5;196m"
  DIM="${CSI}2m"
  BOLD="${CSI}1m"
  SELECT="${CSI}1;38;5;16;48;5;82m"
  RESET="${CSI}0m"
fi

restore_terminal() {
  printf '%s' "${RESET}"
  if [[ -n "$CSI" ]]; then
    printf '%s' "${CSI}?25h"
  fi
}

trap restore_terminal EXIT INT TERM

clear_screen() {
  if [[ -n "$CSI" ]]; then
    printf '%s' "${CSI}2J${CSI}H"
  else
    printf '\n\n'
  fi
}

rule() {
  local fill
  printf -v fill '%*s' 98 ''
  printf '+%s+\n' "${fill// /-}"
}

center_line() {
  local text="$1"
  local padding=$(( (100 - ${#text}) / 2 ))
  (( padding < 0 )) && padding=0
  printf '%*s%s\n' "$padding" '' "$text"
}

header() {
  local step="$1"
  local title="$2"
  local completed=0
  [[ "$step" =~ ^[0-9]+$ ]] && completed=$step
  (( completed > 10 )) && completed=10
  local meter=""
  local i
  for (( i=1; i<=10; i++ )); do
    if (( i <= completed )); then meter+="#"; else meter+="-"; fi
  done
  clear_screen
  printf '%b' "$GREEN"
  rule
  printf '%b| %-83s STEP %2s / 10 |%b\n' \
    "$BOLD" "WAYKEEPER(TM) CIV-INSTALL v$VERSION // $title" "$step" "$RESET"
  printf '%b|  MODE GUIDED / LOCAL-ONLY     PROGRESS [%-10s] %3d%%     TARGET %-27s |%b\n' \
    "$DIM" "$meter" "$((completed * 10))" "$(basename "$TARGET")" "$RESET"
  printf '%b' "$GREEN"
  rule
  printf '%b' "$RESET"
}

splash() {
  header 0 "CIVILIZATION BOOTSTRAP"
  printf '%b' "$GREEN$BOLD"
  printf '%s\n' \
    '      __        __    _  __   __ _  __ _____ _____ ____  _____ ____' \
    '      \ \      / /   / \ \ \ / /| |/ /| ____| ____|  _ \| ____|  _ \' \
    '       \ \ /\ / /   / _ \ \ V / | '\'' / |  _| |  _| | |_) |  _| | |_) |' \
    '        \ V  V /   / ___ \ | |  | . \ | |___| |___|  __/| |___|  _ <' \
    '         \_/\_/   /_/   \_\|_|  |_|\_\|_____|_____|_|   |_____|_| \_\'
  printf '%b' "$RESET"
  printf '\n'
  printf '%b' "$AMBER$BOLD"
  center_line 'CIVILIZATION INSTALLER // GUIDED MODE'
  printf '%b' "$GREEN"
  center_line 'SURVIVE // PRESERVE // REBUILD'
  printf '%b' "$RESET"
  rule
  printf '%bTarget:%b %s\n\n' "$DIM" "$RESET" "$TARGET"
  printf 'This installer creates a new encampment record tree.\n'
  printf 'It will not overwrite an existing target.\n\n'
  printf 'Press Enter to begin, or Q to quit: '
  local key=""
  IFS= read -rsn1 key || true
  printf '\n'
  case "$key" in
    q|Q) exit 0 ;;
  esac
}

draw_footer() {
  printf '\n%b' "$GREEN"
  rule
  printf '%b' "$DIM"
  center_line '[UP/DOWN OR J/K] CHOOSE     [ENTER] ACCEPT     [B] BACK     [Q] QUIT'
  printf '%b' "$GREEN"
  rule
  printf '%b' "$RESET"
}

choose_menu() {
  local step="$1"
  local title="$2"
  local command="$3"
  local body="$4"
  local current="$5"
  shift 5
  local items=("$@")
  local selected=0
  local i key seq

  for (( i=0; i<${#items[@]}; i++ )); do
    if [[ "${items[$i]}" == "$current" ]]; then
      selected="$i"
      break
    fi
  done

  while true; do
    header "$step" "$title"
    printf '%b$ %s%b\n\n' "$AMBER" "$command" "$RESET"
    printf '%s\n\n' "$body"

    for (( i=0; i<${#items[@]}; i++ )); do
      if (( i == selected )); then
        printf '%b  > [%s] %-82s%b\n' "${SELECT:-$GREEN$BOLD}" \
          "$(printf "\\$(printf '%03o' "$((65+i))")")" "${items[$i]}" "$RESET"
      else
        printf '    [%s] %s\n' \
          "$(printf "\\$(printf '%03o' "$((65+i))")")" "${items[$i]}"
      fi
    done
    draw_footer

    key=""
    IFS= read -rsn1 key || true
    case "$key" in
      "")
        MENU_RESULT="${items[$selected]}"
        return 0
        ;;
      q|Q)
        return 20
        ;;
      b|B)
        return 10
        ;;
      j|J)
        selected=$(( (selected + 1) % ${#items[@]} ))
        ;;
      k|K)
        selected=$(( (selected - 1 + ${#items[@]}) % ${#items[@]} ))
        ;;
      1|2|3|4|5|6|7|8|9)
        i=$((key - 1))
        if (( i < ${#items[@]} )); then
          selected="$i"
        fi
        ;;
      $'\033')
        seq=""
        IFS= read -rsn2 -t 0.15 seq || true
        case "$seq" in
          '[A') selected=$(( (selected - 1 + ${#items[@]}) % ${#items[@]} )) ;;
          '[B') selected=$(( (selected + 1) % ${#items[@]} )) ;;
        esac
        ;;
    esac
  done
}

body_census() {
  printf '%s\n' \
    '[ OK ] atmospheric event has stopped getting worse' \
    '[ OK ] 23 living persons detected' \
    '[ OK ] 2 goats detected' \
    '[WARN] generator is making theological noises' \
    '' \
    'Initialize /etc/people with:' \
    '  name, call sign, age band, household, dependents, next-of-kin,' \
    '  skills, restrictions, medical alert, assigned bed, consent status.' \
    '' \
    'Rule: no person becomes invisible because their name was not written down.' \
    'Required media: bound paper plus an offline terminal copy.'
}

body_sanctuary() {
  printf '%s\n' \
    'CHURCH PREFLIGHT // REBUILD, DO NOT MERELY RESTORE' \
    '' \
    '[CHECK] structure, roof, fire exits, cellar, ventilation' \
    '[CHECK] radiation, potable water, latrine distance' \
    '[CHECK] sleeping capacity and accessibility' \
    '' \
    'The church becomes worship space, assembly hall, archive, warming center,' \
    'alarm bell, radio mast, memorial, and shelter.' \
    '' \
    'Doctrine may guide the town; it may not erase a person.' \
    'Everyone may enter during fire, storm, attack, or lethal cold.'
}

body_doctrine() {
  printf '%s\n' \
    'FOUNDING COVENANT // REQUIRED ARTICLES' \
    '' \
    '01. Life outranks property.' \
    '02. Emergency food, water, and shelter are not leverage.' \
    '03. No punishment without charge, witness, and appeal.' \
    '04. No inherited office; no permanent emergency power.' \
    '05. Public stores require public accounting.' \
    '06. Medical records remain private.' \
    '07. Children are never collateral for household debt.' \
    '08. The charter may be amended, but never in secret.' \
    '' \
    'Anyone declaring themselves king because they found the clipboard' \
    'will be assigned latrine inventory.'
}

body_governance() {
  printf '%s\n' \
    'POPULATE /ETC/OFFICES' \
    '' \
    'Required offices:' \
    '  steward, quartermaster, water keeper, medic, works lead,' \
    '  archivist, watch captain, cook, teacher, dispute clerk.' \
    '' \
    'Every office requires:' \
    '  a deputy, ledger, term or review date, conflict declaration,' \
    '  removal procedure, and handoff checklist.' \
    '' \
    'Roles are responsibilities. They are not hereditary ranks.'
}

body_bundle() {
  printf '%s\n' \
    'SELECT INSTITUTION BUNDLE' \
    '' \
    '[01] sanctuary.service .... assembly, shelter, archive' \
    '[02] water.target ......... test, well, storage, distribution' \
    '[03] sanitation.target .... latrines, waste, burial protocol' \
    '[04] clinic.service ....... triage, pharmacy, quarantine' \
    '[05] kitchen.service ...... calories, fuel, clean vessels' \
    '[06] storehouse.mount ..... food, tools, seed, controlled issue' \
    '[07] workshop.service ..... repair, salvage, fabrication' \
    '[08] school.timer ......... literacy, trades, civic memory' \
    '[09] market.socket ........ barter, notices, dispute desk' \
    '[10] saloon.service ....... meals, music, neutral table' \
    '' \
    'Install order is safety order, not prestige order.'
}

body_ledgers() {
  printf '%s\n' \
    'INITIALIZE PUBLIC AND RESTRICTED LEDGERS' \
    '' \
    'PUBLIC: stores, rations, labor, decisions, votes, tools, incidents.' \
    'RESTRICTED: medical, protected witnesses, child safeguarding,' \
    '            infrastructure vulnerabilities.' \
    '' \
    'Daily close:' \
    '  number pages -> total stores -> append corrections -> sign -> duplicate.' \
    '' \
    'Erasing a debt may occur through due process.' \
    'Erasing the historical entry may not. Corrections append.'
}

body_watch() {
  printf '%s\n' \
    'CONFIGURE WATCH + JUSTICE' \
    '' \
    'Enable paired patrols, timed check-ins, weapons checkout,' \
    'use-of-force reports, public charges, review, and appeal.' \
    '' \
    'Disable collective punishment, secret detention, ration denial,' \
    'and permanent curfew without recorded renewal.' \
    '' \
    'Security protects the settlement. It does not become a second government.'
}

body_saloon() {
  printf '%s\n' \
    'SALOON.SERVICE // THE LAST DEPENDENCY' \
    '' \
    'Provides hot meals, clean water, music, notices, trade, and a neutral table.' \
    'Requires water, sanitation, kitchen, stores audit, watch, and appeal.' \
    '' \
    'House rules:' \
    '  no ration-credit drinking; no weapons at the dispute table;' \
    '  intoxicated promises do not amend the charter;' \
    '  the bottle is not currency before noon.' \
    '' \
    '[ OK ] piano status: technically repairable'
}

store_choice() {
  local variable="$1"
  shift
  choose_menu "$@"
  local rc=$?
  if (( rc == 0 )); then
    printf -v "$variable" '%s' "$MENU_RESULT"
  fi
  return "$rc"
}

signature_screen() {
  header 7 "SIGNATURES + SEAL"
  printf '%b$ signctl enroll --witnessed --read-aloud%b\n\n' "$AMBER" "$RESET"
  printf '%s\n' \
    'The charter must be read aloud before signing.' \
    'A person unable to write may make a witnessed mark.' \
    'A person may refuse office without losing rations.' \
    'Each signature binds the office, not the bloodline.' \
    '' \
    'Type /back to return to the previous step or /quit to exit.' \
    ''

  printf 'Founding steward name or witnessed mark: '
  IFS= read -r STEWARD
  case "$STEWARD" in
    /back) return 10 ;;
    /quit) return 20 ;;
    "") printf '%bA steward signature is required.%b\n' "$RED" "$RESET"; return 30 ;;
  esac

  printf 'Public witness name or witnessed mark: '
  IFS= read -r WITNESS
  case "$WITNESS" in
    /back) return 10 ;;
    /quit) return 20 ;;
    "") printf '%bA witness signature is required.%b\n' "$RED" "$RESET"; return 30 ;;
  esac

  choose_menu 7 "SIGNATURES + SEAL" \
    "signctl set-method" \
    "Choose how the founding record will describe these signatures." \
    "$SIGNATURE_METHOD" \
    "Names + witnesses" "Marks + witnesses" "Voice + paper record"
  local rc=$?
  if (( rc == 0 )); then
    SIGNATURE_METHOD="$MENU_RESULT"
  fi
  return "$rc"
}

conf_escape() {
  local value="$1"
  value="${value//\\/\\\\}"
  value="${value//\"/\\\"}"
  printf '%s' "$value"
}

validate_target() {
  if [[ -e "$TARGET" ]]; then
    printf '%bRefusing to overwrite existing target:%b %s\n' "$RED" "$RESET" "$TARGET" >&2
    return 1
  fi
  return 0
}

write_public_files() {
  local stamp="$1"
  local base="$TARGET"

  mkdir -p \
    "$base/etc/civilization" \
    "$base/var/lib/waykeeper/ledgers" \
    "$base/var/lib/waykeeper/operations" \
    "$base/var/lib/waykeeper/rosters" \
    "$base/var/lib/waykeeper/signatures" \
    "$base/var/lib/waykeeper/restricted" \
    "$base/var/log/waykeeper"

  {
    printf 'HOSTNAME="waykeeper-encampment"\n'
    printf 'CREATED_UTC="%s"\n' "$stamp"
    printf 'CENSUS="%s"\n' "$(conf_escape "$CENSUS")"
    printf 'SANCTUARY="%s"\n' "$(conf_escape "$SANCTUARY")"
    printf 'DOCTRINE="%s"\n' "$(conf_escape "$DOCTRINE")"
    printf 'GOVERNANCE="%s"\n' "$(conf_escape "$GOVERNANCE")"
    printf 'BUNDLE="%s"\n' "$(conf_escape "$BUNDLE")"
    printf 'LEDGERS="%s"\n' "$(conf_escape "$LEDGERS")"
    printf 'SIGNATURE_METHOD="%s"\n' "$(conf_escape "$SIGNATURE_METHOD")"
    printf 'WATCH="%s"\n' "$(conf_escape "$WATCH")"
    printf 'SALOON="%s"\n' "$(conf_escape "$SALOON")"
  } > "$base/etc/civilization/civilization.conf"

  {
    printf '%s\n' \
      'THE WAYKEEPER FOUNDING COVENANT' \
      '' \
      "Doctrine profile: $DOCTRINE" \
      "Governance profile: $GOVERNANCE" \
      '' \
      'ARTICLE I — LIFE' \
      'Life, emergency care, potable water, basic food, and immediate shelter' \
      'outrank property claims, debts, office, and faction.' \
      '' \
      'ARTICLE II — PERSONHOOD AND CONSENT' \
      'No person becomes property, hereditary labor, collateral, or punishment' \
      'for the act or debt of another.' \
      '' \
      'ARTICLE III — DUE PROCESS' \
      'No punishment occurs without a known charge, recorded evidence, a fair' \
      'hearing, a written disposition, and an appeal path.' \
      '' \
      'ARTICLE IV — LIMITED OFFICE' \
      'Every office has a defined duty, review or expiry, deputy, ledger,' \
      'conflict declaration, removal method, and handoff.' \
      '' \
      'ARTICLE V — COMMON STORES' \
      'Public stores require public counts, receipts, corrections, witnesses,' \
      'and scheduled audits. Emergency aid is never coercive leverage.' \
      '' \
      'ARTICLE VI — PRIVACY' \
      'Medical, safeguarding, protected-witness, and critical vulnerability' \
      'records are restricted while aggregate civic decisions remain visible.' \
      '' \
      'ARTICLE VII — RECORD INTEGRITY' \
      'Corrections append. Pages remain numbered. Entries are not secretly' \
      'destroyed. Debt may be forgiven without falsifying the historical record.' \
      '' \
      'ARTICLE VIII — AMENDMENT' \
      'The covenant may be amended only after notice, public reading, recorded' \
      'deliberation, a defined vote, signatures, and an effective date.' \
      '' \
      'ARTICLE IX — EMERGENCY POWER' \
      'Emergency authority expires automatically unless renewed through the' \
      'recorded charter process. Necessity is not a permanent constitution.' \
      '' \
      'ARTICLE X — RETURN' \
      'The purpose of survival is not merely continuation. It is the restoration' \
      'of human dignity, memory, responsibility, learning, and home.'
  } > "$base/etc/civilization/charter.txt"

  {
    printf '%s\n' \
      'WAYKEEPER ENCAMPMENT — READ THIS FIRST' \
      '' \
      '1. Read the charter aloud.' \
      '2. Complete the people register before assigning labor.' \
      '3. Test water and establish sanitation before opening public food service.' \
      '4. Number all paper ledger pages and duplicate the daily close.' \
      '5. Keep restricted records locked and separate from public ledgers.' \
      '6. Review emergency authority on its written expiry date.' \
      '7. Do not start saloon.service before its safety dependencies.' \
      '' \
      'Survive // Preserve // Rebuild'
  } > "$base/README-FIRST.txt"

  {
    printf '%s\n' \
      '01 sanctuary.service — worship, assembly, archive, emergency shelter' \
      '02 water.target — test, source, storage, distribution' \
      '03 sanitation.target — latrines, waste, burial protocol' \
      '04 clinic.service — triage, pharmacy, quarantine' \
      '05 kitchen.service — calories, fuel, clean vessels' \
      '06 storehouse.mount — food, tools, seed, controlled issue' \
      '07 workshop.service — repair, salvage, fabrication' \
      '08 school.timer — literacy, trades, civic memory' \
      '09 market.socket — barter, notices, dispute desk' \
      '10 saloon.service — meals, music, trade, neutral table'
  } > "$base/var/lib/waykeeper/build-order.txt"

  {
    printf '%s\n' \
      'WAYKEEPER ENCAMPMENT — OPERATIONS LIST' \
      '' \
      'Use this as a shift board. Record the named operator and completion time' \
      'in operations.tsv; do not mark a task complete from assumption or hearsay.' \
      '' \
      'P0 // LIFE SAFETY — OPEN IMMEDIATELY' \
      '[ ] Account for every resident, visitor, dependent, and missing person.' \
      '[ ] Establish a potable-water source; test, treat, label, and protect it.' \
      '[ ] Establish latrines, handwashing, waste separation, and burial protocol.' \
      '[ ] Open triage; record urgent conditions, allergies, medications, and isolation.' \
      '[ ] Inspect shelter for fire, collapse, weather, ventilation, and accessibility.' \
      '[ ] Post alarms, evacuation routes, rally points, and around-the-clock fire watch.' \
      '' \
      'P1 // STABILIZE — FIRST OPERATIONAL PERIOD' \
      '[ ] Count food, fuel, batteries, tools, radios, medicines, and shelter capacity.' \
      '[ ] Establish ration rules, issue receipts, minimum reserves, and daily close.' \
      '[ ] Establish radio schedule, message log, call signs, and runner fallback.' \
      '[ ] Assign primary and deputy operators for water, medical, stores, watch, and cook.' \
      '[ ] Start paired watch rotation with check-ins, incident reports, and relief times.' \
      '[ ] Publish the next briefing time and unresolved hazards list.' \
      '' \
      'P2 // GOVERN — AFTER IMMEDIATE SAFETY' \
      '[ ] Read the covenant aloud; record consent, objections, amendments, and witnesses.' \
      '[ ] Fill offices with expiry/review dates, deputies, removal process, and handoff.' \
      '[ ] Separate public ledgers from medical, safeguarding, and vulnerability records.' \
      '[ ] Open a dispute desk with notice, evidence, written disposition, and appeal.' \
      '[ ] Begin school, apprenticeship, repair, seed, archive, and maintenance registers.' \
      '[ ] Review emergency authority before expiry; append decisions instead of erasing.' \
      '' \
      'DAILY CLOSE' \
      '[ ] Reconcile people, potable water, calories, medical cases, stores, power, and watch.' \
      '[ ] Append corrections, sign each ledger, duplicate critical pages, and brief relief.' \
      '[ ] Publish tomorrow priorities without exposing protected personal information.'
  } > "$base/OPERATIONS-LIST.txt"

  {
    printf '%s\n' 'priority,operation,status,primary,deputy,opened_utc,due_utc,closed_utc,evidence,notes'
    printf '%s\n' \
      'P0,Account for all people,OPEN,,,,,,,' \
      'P0,Verify potable water,OPEN,,,,,,,' \
      'P0,Establish sanitation,OPEN,,,,,,,' \
      'P0,Open medical triage,OPEN,,,,,,,' \
      'P0,Inspect shelter and fire safety,OPEN,,,,,,,' \
      'P0,Post evacuation and rally plan,OPEN,,,,,,,' \
      'P1,Count critical stores,OPEN,,,,,,,' \
      'P1,Establish communications schedule,OPEN,,,,,,,' \
      'P1,Assign essential service rotations,OPEN,,,,,,,' \
      'P1,Publish operational briefing,OPEN,,,,,,,' \
      'P2,Read and review covenant,OPEN,,,,,,,' \
      'P2,Open ledgers and dispute process,OPEN,,,,,,,' \
      'P2,Begin learning and repair registers,OPEN,,,,,,,' \
      'DAILY,Close and duplicate critical records,RECURRING,,,,,,,'
  } > "$base/var/lib/waykeeper/operations/operations.tsv"

  printf '%s\n' \
    'person_id,legal_name,chosen_name,call_sign,age_band,household,dependents,skills,restrictions,medical_alert,assigned_bed,next_of_kin,consent_status,status' \
    > "$base/var/lib/waykeeper/rosters/people.csv"

  {
    printf '%s\n' 'office,holder,deputy,term_start,review_or_expiry,ledger,custody_handoff,status'
    printf '%s\n' \
      'steward,,,,,decisions.log,,OPEN' \
      'quartermaster,,,,,stores.csv,,OPEN' \
      'water_keeper,,,,,water-tests.csv,,OPEN' \
      'medic,,,,,restricted/medical-register.csv,,OPEN' \
      'works_lead,,,,,maintenance.csv,,OPEN' \
      'archivist,,,,,archive-register.csv,,OPEN' \
      'watch_captain,,,,,incidents.log,,OPEN' \
      'cook,,,,,rations.csv,,OPEN' \
      'teacher,,,,,school-register.csv,,OPEN' \
      'dispute_clerk,,,,,decisions.log,,OPEN'
  } > "$base/var/lib/waykeeper/rosters/offices.csv"

  printf '%s\n' 'timestamp,item,unit,opening,received,issued,loss,closing,custodian,witness,reference' \
    > "$base/var/lib/waykeeper/ledgers/stores.csv"
  printf '%s\n' 'timestamp,person_or_household,item,quantity,reason,issuer,recipient_mark,witness' \
    > "$base/var/lib/waykeeper/ledgers/rations.csv"
  printf '%s\n' 'date,person,task,hours_or_units,accommodation,supervisor,person_mark,notes' \
    > "$base/var/lib/waykeeper/ledgers/labor.csv"
  printf '%s\n' 'timestamp,tool_id,description,issued_to,condition_out,due,returned,condition_in,custodian' \
    > "$base/var/lib/waykeeper/ledgers/tools.csv"
  printf '%s\n' 'timestamp | decision_id | notice | question | vote | disposition | effective_date | appeal_or_review | signatures' \
    > "$base/var/lib/waykeeper/ledgers/decisions.log"
  printf '%s\n' 'timestamp | incident_id | reporter | protected_ref | allegation | evidence | immediate_action | reviewer | disposition | appeal' \
    > "$base/var/lib/waykeeper/ledgers/incidents.log"
  printf '%s\n' 'timestamp,test_point,test_type,result,unit,safe_range,tester,witness,action' \
    > "$base/var/lib/waykeeper/ledgers/water-tests.csv"
  printf '%s\n' 'timestamp,asset,condition,work_required,parts,custodian,completed,witness' \
    > "$base/var/lib/waykeeper/ledgers/maintenance.csv"

  {
    printf '%s\n' \
      'FOUNDING SIGNATURE RECORD' \
      '' \
      "Created UTC: $stamp" \
      "Method: $SIGNATURE_METHOD" \
      "Founding steward: $STEWARD" \
      "Public witness: $WITNESS" \
      '' \
      'Attestation:' \
      'The covenant was presented for reading. The office duties, limits,' \
      'review dates, record obligations, and refusal rights were disclosed.' \
      'These marks bind the stated offices and do not bind descendants.'
  } > "$base/var/lib/waykeeper/signatures/founding-signatures.txt"

  printf '%s\n' \
    'person_id,medical_alert,care_plan,medications,allergies,clinician,last_review,access_log' \
    > "$base/var/lib/waykeeper/restricted/medical-register.csv"
  printf '%s\n' \
    'protected_id,contact_method,safeguarding_need,custodian,access_log' \
    > "$base/var/lib/waykeeper/restricted/protected-witnesses.csv"

  {
    printf '%s\n' \
      "[$stamp] WAYKEEPER CIV-INSTALL v$VERSION" \
      "[$stamp] target=$TARGET" \
      "[$stamp] doctrine=$DOCTRINE" \
      "[$stamp] governance=$GOVERNANCE" \
      "[$stamp] institution_bundle=$BUNDLE" \
      "[$stamp] record_system=$LEDGERS" \
      "[$stamp] operations_list=OPERATIONS-LIST.txt" \
      "[$stamp] status=installed_with_warnings"
  } > "$base/var/log/waykeeper/install.log"

  chmod 700 "$base/var/lib/waykeeper/signatures" "$base/var/lib/waykeeper/restricted"
  chmod 600 \
    "$base/var/lib/waykeeper/signatures/founding-signatures.txt" \
    "$base/var/lib/waykeeper/restricted/medical-register.csv" \
    "$base/var/lib/waykeeper/restricted/protected-witnesses.csv"
}

install_encampment() {
  validate_target || return 1
  local stamp
  stamp="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"

  write_public_files "$stamp" || return 1

  header 10 "COMMITTING CIVILIZATION"
  printf '%b$ waykeeper-install --commit %s%b\n\n' "$AMBER" "$TARGET" "$RESET"
  printf '%b[ OK ]%b charter written and witnessed\n' "$GREEN" "$RESET"
  printf '%b[ OK ]%b people and offices registers initialized\n' "$GREEN" "$RESET"
  printf '%b[ OK ]%b ledgers numbered by schema and separated by access\n' "$GREEN" "$RESET"
  printf '%b[ OK ]%b operations list dropped and shift board initialized\n' "$GREEN" "$RESET"
  printf '%b[ OK ]%b sanctuary.service queued\n' "$GREEN" "$RESET"
  printf '%b[ OK ]%b water.target declared as dependency\n' "$GREEN" "$RESET"
  printf '%b[ OK ]%b saloon.service held behind safety dependencies\n' "$GREEN" "$RESET"
  printf '\n%bCIVILIZATION INSTALLED WITH 3 WARNINGS:%b\n' "$AMBER" "$RESET"
  printf '%s\n' \
    '  1. People are not packages.' \
    '  2. Emergency powers expire.' \
    '  3. The jukebox remains unverified.' \
    '' \
    "Settlement record: $TARGET" \
    "Operations list: $TARGET/OPERATIONS-LIST.txt" \
    '' \
    'WELCOME HOME, WAYKEEPER.' \
    'Survive // Preserve // Rebuild'
}

review_screen() {
  while true; do
    header 10 "REVIEW + COMMIT"
    printf '%b$ waykeeper-install --write-config civilization.conf%b\n\n' "$AMBER" "$RESET"
    printf '%s\n' \
      "TARGET=\"$TARGET\"" \
      "CENSUS=\"$CENSUS\"" \
      "SANCTUARY=\"$SANCTUARY\"" \
      "DOCTRINE=\"$DOCTRINE\"" \
      "GOVERNANCE=\"$GOVERNANCE\"" \
      "BUNDLE=\"$BUNDLE\"" \
      "LEDGERS=\"$LEDGERS\"" \
      "SIGNATURE_METHOD=\"$SIGNATURE_METHOD\"" \
      "WATCH=\"$WATCH\"" \
      "SALOON=\"$SALOON\"" \
      "STEWARD=\"$STEWARD\"" \
      "WITNESS=\"$WITNESS\"" \
      '' \
      '[ OK ] every office expires or faces review' \
      '[ OK ] every stores issue produces a receipt' \
      '[ OK ] every punishment has an appeal path' \
      '[ OK ] saloon waits for water, clinic, sanitation, and audit' \
      '[WARN] civilization contains people; unattended upgrades disabled' \
      ''
    printf '%b[C]%b Commit install   %b[B]%b Back   %b[Q]%b Quit\n' \
      "$GREEN" "$RESET" "$DIM" "$RESET" "$DIM" "$RESET"
    local key=""
    IFS= read -rsn1 key || true
    case "$key" in
      c|C|"") install_encampment; return $? ;;
      b|B) return 10 ;;
      q|Q) return 20 ;;
    esac
  done
}

run_interactive() {
  splash
  local step=1
  local rc=0

  while (( step <= 10 )); do
    rc=0
    case "$step" in
      1)
        store_choice CENSUS 1 "RESCUE CENSUS" \
          "censusctl init --redundant" "$(body_census)" "$CENSUS" \
          "Census first" "Households first" "Unknown arrivals mode" || rc=$?
        ;;
      2)
        store_choice SANCTUARY 2 "REBUILD THE CHURCH" \
          "mount /dev/sanctuary /srv/community" "$(body_sanctuary)" "$SANCTUARY" \
          "Sanctuary + assembly" "Civic commons" "Memorial + archive" || rc=$?
        ;;
      3)
        store_choice DOCTRINE 3 "FOUNDING DOCTRINE" \
          "covenantctl configure /etc/civilization/charter.conf" "$(body_doctrine)" "$DOCTRINE" \
          "Charter of mercy" "Charter of duty" "Hybrid covenant" || rc=$?
        ;;
      4)
        store_choice GOVERNANCE 4 "PEOPLE + OFFICES" \
          "useradd --system civilization && rolectl assign" "$(body_governance)" "$GOVERNANCE" \
          "30-day incident command" "Rotating council" "Steward + elected council" || rc=$?
        ;;
      5)
        store_choice BUNDLE 5 "INSTALL INSTITUTIONS" \
          "pacstrap /mnt/civilization survival-core community-tools" "$(body_bundle)" "$BUNDLE" \
          "Survival core" "Core + learning" "Full settlement" || rc=$?
        ;;
      6)
        store_choice LEDGERS 6 "CIVIC LEDGERS" \
          "ledgerctl init --append-only --daily-close" "$(body_ledgers)" "$LEDGERS" \
          "Dual paper + terminal" "Paper primary" "Terminal + print close" || rc=$?
        ;;
      7)
        signature_screen || rc=$?
        ;;
      8)
        store_choice WATCH 8 "WATCH + JUSTICE" \
          "systemctl enable watch@rotation justice@appeal" "$(body_watch)" "$WATCH" \
          "Community watch" "Guard rotation" "Hardened perimeter" || rc=$?
        ;;
      9)
        store_choice SALOON 9 "OPEN THE SALOON" \
          "systemctl enable --now saloon.service" "$(body_saloon)" "$SALOON" \
          "Dry commons" "Managed tavern" "Night saloon" || rc=$?
        ;;
      10)
        review_screen || rc=$?
        if (( rc == 0 )); then
          return 0
        fi
        ;;
    esac

    case "$rc" in
      0) step=$((step + 1)) ;;
      10)
        if (( step > 1 )); then
          step=$((step - 1))
        fi
        ;;
      20)
        clear_screen
        printf 'Installation cancelled. No settlement files were written.\n'
        return 0
        ;;
      30)
        ;;
      *)
        printf 'Unexpected installer state: %s\n' "$rc" >&2
        return "$rc"
        ;;
    esac
  done
}

run_defaults() {
  if [[ -z "$STEWARD" || -z "$WITNESS" ]]; then
    printf '%s\n' \
      '--defaults requires signatures in environment variables:' \
      '  WAYKEEPER_STEWARD="Name" WAYKEEPER_WITNESS="Name" \' \
      '    bash waykeeper-civ-install.sh --defaults --target ./encampment' >&2
    return 2
  fi
  install_encampment
}

if (( DEFAULTS_MODE == 1 )); then
  run_defaults
else
  run_interactive
fi
