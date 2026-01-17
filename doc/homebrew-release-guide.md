# Homebrew Release Guide

This guide explains how to release **Deskflow HID** via Homebrew, enabling users to easily install the application on macOS. The application is distributed as a single **Universal DMG**, which contains both Intel (`x86_64`) and Apple Silicon (`arm64`) architectures in a single package.

## Prerequisites

1.  **GitHub Account**: You need a GitHub account to host the Tap repository.
2.  **Release Artifact**: You must have `Deskflow-HID-x.y.z-universal.dmg` uploaded to a public location (e.g., GitHub Releases).
3.  **SHA256 Checksum**: You need the SHA256 hash of the universal DMG file.

## 1. Create a Homebrew Tap Repository

Homebrew allows you to create your own package repository called a "Tap".

1.  Create a **new public GitHub repository**.
2.  Name it with the prefix `homebrew-`. This is required for the shorthand `brew tap <user>/<repo>` command to work.
    *   **Recommended Name**: `homebrew-deskflow`
    *   **Resulting Tap Command**: `brew tap lockekk/deskflow`

## 2. Create the Cask File

In your new `homebrew-deskflow` repository, create the following directory structure and file:

```text
homebrew-deskflow/
└── Casks/
    └── deskflow-hid.rb
```

### Cask Content (`deskflow-hid.rb`)

Copy the following Ruby code into `deskflow-hid.rb`. This version is simplified because we use a single universal artifact.

```ruby
cask "deskflow-hid" do
  version "1.0.0" # <--- UPDATE THIS FOR EVERY RELEASE
  sha256 "REPLACE_WITH_UNIVERSAL_DMG_SHA256"

  url "https://github.com/lockekk/deskflow-hid/releases/download/v#{version}/Deskflow-HID-#{version}-macos-universal.dmg"
  name "Deskflow HID"
  desc "Control utility for Deskflow HID devices"
  homepage "https://github.com/lockekk/deskflow-hid"

  # The name of the app bundle inside the DMG.
  # This MUST match the actual .app name in your DMG.
  app "Deskflow-HID.app"

  # Standard cleanup for uninstallation (Optional but good practice)
  zap trash: [
    "~/Library/Preferences/com.deskflow.hid.plist",
    "~/Library/Saved Application State/com.deskflow.hid.savedState"
  ]
end
```

## 3. How Users Install

Once you have pushed this file to your `homebrew-deskflow` repository, users can install the application using two standard commands:

### Step 1: Add the Tap
```bash
brew tap lockekk/deskflow
```
*Note: Homebrew automatically assumes the `homebrew-` prefix for the repo name.*

### Step 2: Install the Application
```bash
brew install --cask deskflow-hid
```

Homebrew will download the universal DMG, verify the SHA256 hash, and install `Deskflow-HID.app` to their `/Applications` folder.

## 4. Maintenance (Releasing Updates)

When you release a new version (e.g., `1.0.1`), follow these steps:

1.  **Tag and Release**: Create a new release in your main project repo (`deskflow-hid`) and upload the new universal DMG.
2.  **Calculate Hash**: Get the SHA256 checksum for the new file.
    ```bash
    shasum -a 256 Deskflow-HID-1.0.1-macos-universal.dmg
    ```
3.  **Update Cask**: Edit `Casks/deskflow-hid.rb` in your `homebrew-deskflow` repo:
    *   Update `version "1.0.1"`
    *   Update the `sha256` value.
4.  **Push**: Commit and push the changes. Users can now run `brew upgrade deskflow-hid` to get the latest version.
