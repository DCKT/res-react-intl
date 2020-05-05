#!/usr/bin/env node

const { execSync } = require('child_process')
const fs = require('fs')
const path = require('path')
const esy = require('../esy.json')

console.log(`Using esy.json:\n${esy}`)

const filesToCopy = ['LICENSE', 'README.md', 'vendors/ReactIntl.re']

function exec(cmd) {
  console.log(`exec: ${cmd}`)
  return execSync(cmd).toString()
}

function mkdirpSync(p) {
  if (fs.existsSync(p)) {
    return
  }
  mkdirpSync(path.dirname(p))
  fs.mkdirSync(p)
}

function removeSync(p) {
  exec(`rm -rf -i "${p}"`)
}

const src = path.resolve(path.join(__dirname, '..'))
const dst = path.resolve(path.join(__dirname, '..', '_release'))

mkdirpSync(dst)
removeSync(dst)
mkdirpSync(dst)

for (const file of filesToCopy) {
  const p = path.join(dst, file)
  mkdirpSync(path.dirname(p))
  fs.copyFileSync(path.join(src, file), p)
}

fs.copyFileSync(path.join(src, 'script', 'release-postinstall.js'), path.join(dst, 'postinstall.js'))

const filesToTouch = ['react-intl-auto-id-ppx.exe']

for (const file of filesToTouch) {
  const p = path.join(dst, file)
  mkdirpSync(path.dirname(p))
  fs.writeFileSync(p, '')
}

const pkgJson = {
  name: 'react-intl-auto-id-ppx',
  version: esy.version,
  description: esy.description,
  homepage: esy.homepage,
  license: esy.license,
  repository: esy.repository,
  scripts: {
    postinstall: 'node postinstall.js',
  },
  bin: {
    'react-intl-auto-id-ppx': 'react-intl-auto-id-ppx.exe',
  },
  files: [
    'platform-windows-x64/',
    'platform-linux-x64/',
    'platform-darwin-x64/',
    'postinstall.js',
    'react-intl-auto-id-ppx.exe',
  ],
}

fs.writeFileSync(path.join(dst, 'package.json'), JSON.stringify(pkgJson, null, 2))
