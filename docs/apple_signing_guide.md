# Production Release Guide: Signing with "Developer ID Application"

This guide is exclusively for **Public Release**. Following these steps will ensure your app is signed correctly for distribution to other users.

## 1. Generate the Certificate (CLI Method)
*Since Keychain Access can be buggy, we use the Terminal.*

1.  **Open Terminal** and run these commands to generate your Request File:
    ```bash
    # 1. Generate Private Key
    openssl genrsa -out private.key 2048

    # 2. Generate CSR (Certificate Signing Request)
    openssl req -new -key private.key -out request.csr -subj "/emailAddress=your_email@example.com/CN=Your Name/C=US"
    ```
    *(Note: `CN` should match your Apple Developer Account Name)*

2.  **Upload to Apple:**
    *   Go to **[Apple Developer: Certificates](https://developer.apple.com/account/resources/certificates/add)**.
    *   Select **"Developer ID Application"**.
    *   Select **"G2 Sub-CA"** (Profile Type).
    *   Upload the `request.csr` file you just created.

3.  **Download:**
    *   Download the certificate (`developerID_application.cer`).
    *   **Move it** to the same folder as your `private.key` (e.g., your Home folder).

## 2. Install the Certificate (CLI Method)
*We must combine the "Public" certificate from Apple with your "Private" key.*

Run these checks/commands:

1.  **Convert & Combine (Critical Step):**
    *   *Note: We use `-legacy` to ensure macOS Keychain accepts the encryption.*
    ```bash
    # Convert .cer to .pem
    openssl x509 -in developerID_application.cer -inform DER -out cert.pem -outform PEM

    # Create .p12 (set password to "1234" when asked, as we use it in the next command)
    openssl pkcs12 -export -legacy -inkey private.key -in cert.pem -out import_me.p12 -passout pass:1234
    ```

2.  **Import to Keychain:**
    ```bash
    security import import_me.p12 -k ~/Library/Keychains/login.keychain-db -P 1234 -T /usr/bin/codesign
    ```

3.  **Clean up** (Optional, but safe now that it's in Keychain):
    ```bash
    rm cert.pem import_me.p12
    # Keep private.key somewhere safe just in case!
    ```

## 3. Verify & Configure
1.  **Check for Validity:**
    ```bash
    security find-identity -v -p codesigning
    ```
    *   **Success:** You see `1) [HASH] "Developer ID Application: ..."`
    *   **Failure (0 identities):** You likely need the Intermediate Certificate (see below).

2.  **Update Build Config:**
    Copy the Hash from the success step above and update your `.zshrc`:
    ```bash
    export APPLE_CODESIGN_DEV="[PASTE_YOUR_HASH_HERE]"
    ```
    Then run `source ~/.zshrc`.

## Troubleshooting: "0 valid identities"
If the certificate is installed but not "valid", you are missing the **Intermediate Certificate**.

1.  Download **[Developer ID G2 Intermediate Certificate](https://www.apple.com/certificateauthority/DeveloperIDG2CA.cer)**.
2.  Install it:
    ```bash
    security add-trusted-cert -d -r trustRoot -k ~/Library/Keychains/login.keychain-db DeveloperIDG2CA.cer
    ```

## 5. Setting up on Another Mac
*You don't need to generate a new certificate from apple. Just move your keys.*

### Option A: Transfer (Recommended)
**Important:** A `.p12` file acts like a zipped bag that holds **BOTH** your Certificate and your Private Key.

1.  **On Mac 1:** Export your certificate:
    *   **GUI:** Keychain Access -> Right-click Certificate -> Export as `.p12`.
    *   **CLI:**
        ```bash
        security export -k ~/Library/Keychains/login.keychain-db -t identities -f pkcs12 -o my_developer_id.p12 -P 1234
        ```
2.  **Transfer:** Copy `my_developer_id.p12` to Mac 2.
3.  **On Mac 2:** Import it:
    *   **Recommendation:** Use the CLI command below. It forces the cert into the **"login"** keychain.
    *   **Warning:** If you double-click the file, **DO NOT** select "System" in the dropdown. Select **"login"**. "System" requires Admin passwords for every build.

    ```bash
    security import my_developer_id.p12 -k ~/Library/Keychains/login.keychain-db -P 1234 -T /usr/bin/codesign

    # REQUIRED: Install the Intermediate Certificate (Apple doesn't include it in p12)
    curl -O https://www.apple.com/certificateauthority/DeveloperIDG2CA.cer
    security add-trusted-cert -d -r trustRoot -k ~/Library/Keychains/login.keychain-db DeveloperIDG2CA.cer
    ```
4.  **Verify & Configure:**
    Run `security find-identity -v -p codesigning` to check if it's there.
    The **Hash** is guaranteed to be the same, so just copy your `APPLE_CODESIGN_DEV` configuration.

### Option B: Start Fresh
If you lost your `.p12` or `private.key`, just follow **Step 1** of this guide again on the new Mac to create a *second* certificate. Apple allows multiple.

