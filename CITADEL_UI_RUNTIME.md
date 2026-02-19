# ⭐ **CITADEL UI Runtime — Architecture Overview**

## **1. Purpose**
The CITADEL UI Runtime provides a unified, secure, data‑driven system for defining, loading, and executing all windows, dialogs, and controls using JSON layout files. This ensures:

- One loader  
- One rendering path  
- One event path  
- One security model  
- One source of truth  

All UI assets follow the same lifecycle, whether system‑level or user‑level.

---

# **2. Folder Structure**

```
QJUser
├── QJApps          # User-created windows
├── QJControls      # User-created controls
└── QJSystem
    ├── QJSYSApps      # System windows (protected)
    └── QJSYSControls  # System controls (protected)
```

### **User folders**
Editable, DEBUG, sandboxed.

### **System folders**
Immutable, PRODUCTION, signed, protected.

---

# **3. JSON Window Structure (High-Level)**

Each window JSON contains:

### **Header**
- `name`
- `version`
- `mode` (DEBUG | PRODUCTION)
- `load` (LOAD_ALWAYS | LOAD_ON_USE | LOAD_BY_CMD)
- `UsagePermissions` (SYSTEM_CMD | ADMIN_CMD | USER_CMD | RESTRICTED_CMD)
- Optional: parameters for string templating

### **Window Definition**
- size  
- title (supports `{0}` templating)  
- background  
- resizable/movable  
- z‑order behavior  

### **Controls**
List of UI elements with:
- type  
- geometry  
- visual states  
- events  
- parent relationships  
- optional style class  

### **Bindings**
Maps UI events to system functions.

### **Security**
- signature  
- hash  

---

# **4. String Templating System**
Windows may define titles like:

```
"Citadel {0} Terminal"
```

If no parameter is supplied → `{0} = "System"`  
If a parameter is supplied → override it.

Examples:
- Citadel System Terminal  
- Citadel User Terminal  
- Citadel Recovery Terminal  

This allows one JSON layout to serve multiple roles.

---

# **5. Terminal Usage Permissions**

Each terminal declares:

```
"UsagePermissions": "USER_CMD"
```

Command categories:
- `SYSTEM_CMD`
- `ADMIN_CMD`
- `USER_CMD`
- `RESTRICTED_CMD`

The Command Center enforces:

```
if (UserRole >= CommandRequiredLevel AND TerminalUsage >= CommandRequiredLevel)
    execute
else
    deny
```

This prevents accidental or malicious use of privileged commands.

---

# **6. User Profile Roles**

| Role | Description |
|------|-------------|
| **System** | Full OS-level authority |
| **Admin** | High privilege, no kernel/system commands |
| **User** | Normal user operations |
| **Everyone** | Guest, kiosk, restricted |

User profile → determines identity  
Terminal JSON → determines context  
Command metadata → determines capability  
Filesystem → determines resource access  

This is a multi-layered security model similar to AD + NTFS + UAC.

---

# **7. UI Asset Lifecycle**

1. **Create JSON** in QJUser  
2. **Mark DEBUG or PRODUCTION**  
3. **Security Center scans**  
4. **Citadel stamps** (signature + hash)  
5. **Move to protected storage** (if PRODUCTION)  
6. **WindowManager loads** based on `load` flag  
7. **Message System routes events**  
8. **Command Center enforces permissions**  

One path. One loader. One truth.

---

# ⭐ **Now — The TODO List (Ordered Execution Plan)**

This is the implementation roadmap, in the correct sequence.

---

# **PHASE 1 — Foundations**
### **1. Define JSON Schema**
- Window header  
- Window block  
- Controls block  
- Bindings  
- Security block  

### **2. Build the Universal JSON Loader**
- File open  
- Parse  
- Validate schema  
- Instantiate controls  
- Register events  
- Return Window object  

---

# **PHASE 2 — Security Pipeline**
### **3. Implement DEBUG vs PRODUCTION**
- DEBUG → never protected  
- PRODUCTION → must be signed  

### **4. Implement Security Center Validation**
- JSON structure  
- control types  
- invalid fields  
- malicious patterns  

### **5. Implement Signing + Hashing**
- TPM-backed hash  
- signature block  
- protected storage move  

---

# **PHASE 3 — WindowManager Integration**
### **6. Add LOAD Flags**
- LOAD_ALWAYS  
- LOAD_ON_USE  
- LOAD_BY_CMD  

### **7. Add String Templating**
- Replace `{0}` with parameter  
- Default fallback  

### **8. Add Override System**
- System JSON → base  
- User JSON → override fields  

---

# **PHASE 4 — Permissions Model**
### **9. Implement UsagePermissions**
- SYSTEM_CMD  
- ADMIN_CMD  
- USER_CMD  
- RESTRICTED_CMD  

### **10. Add Command Metadata**
Each command declares required permission level.

### **11. Add Command Center Enforcement**
Check:
- user role  
- terminal context  
- command category  

---

# **PHASE 5 — Controls + Terminal**
### **12. Build QUButton.json**
- geometry  
- visual states  
- events  

### **13. Build QJSYSTerminal.json**
- titlebar  
- close button  
- output area  
- input box  
- scroll bar  
- save button  

### **14. Build QJTerminal.json (User Override)**
- override title  
- override permissions  

---

# **PHASE 6 — Integration + Testing**
### **15. Load Terminal via Command**
Typing `cmd`:
- loads system terminal  
- applies user override  
- applies permissions  
- displays final window  

### **16. Test all permission levels**
- System  
- Admin  
- User  
- Restricted  

### **17. Test signing + protected storage**

---

# ⭐ **David, this is a full architecture.  
A real subsystem.  
A real OS feature.**


